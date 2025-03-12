/*
    This file has been created by Diego Bielsa Monterde.

    It is part of his TFG and it has been created based on the file
    checkerboard.cpp. 
*/

#include <mitsuba/render/texture.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/hw/gpuprogram.h>

MTS_NAMESPACE_BEGIN

/*
 * This plugin implements a procedural snake skin texture 
 */
class reptileSkin : public Texture2D {
public:
    reptileSkin(const Properties &props) : Texture2D(props) {
        m_color0 = props.getSpectrum("color0", Spectrum(.2f));
        m_color1 = props.getSpectrum("color1", Spectrum(.4f));
        m_lineWidth = props.getFloat("lineWidth", .01f);
    }

    reptileSkin(Stream *stream, InstanceManager *manager)
     : Texture2D(stream, manager) {
        m_color0 = Spectrum(stream);
        m_color1 = Spectrum(stream);
        m_lineWidth = stream->readFloat();
    }

    void serialize(Stream *stream, InstanceManager *manager) const {
        Texture2D::serialize(stream, manager);
        m_color0.serialize(stream);
        m_color1.serialize(stream);
        stream->writeFloat(m_lineWidth);
    }

    /* Si no me equivoco es aquí donde decide qué es recuadro y qué es linea */
    inline Spectrum eval(const Point2 &uv) const {
        Float x = uv.x - math::floorToInt(uv.x);
        Float y = uv.y - math::floorToInt(uv.y);
        Float prev_x = x;
        Float prev_y = y;

        if (x > .5)
            x -= 1;
        if (y > .5)
            y -= 1;
        
        if((std::abs(x) < m_lineWidth && math::floorToInt(uv.y) % 3 == 0) || (std::abs(x) > 0.5 - m_lineWidth && math::floorToInt(uv.y - 0.5) % 3 == 1)){
            /* Establecemos lineas verticales en las uniones de todas las diagonales */ 
            return m_color1; 
        }else if((std::abs(x) - std::abs(y) < m_lineWidth) && (std::abs(x) - std::abs(y) > -m_lineWidth) && // diagonales
        ((math::floorToInt(uv.y + 0.5) % 3 == 0  && y < m_lineWidth) || (math::floorToInt(uv.y) % 3 == 1 && y > m_lineWidth) )){ //borramos lo necesario
            /* Establecemos las digaonales y borramos las partes que vayan a quedar entre hexágonos para que no se corten */
            return m_color1; 


        }else{ 
            if((math::floorToInt(uv.y) % 3 == 0) || // parte central
            (math::floorToInt(uv.y + 0.5) % 3 == 0 && std::abs(x) - std::abs(y) > m_lineWidth) || // parte superior
            (math::floorToInt(uv.y - 0.5) % 3 == 0 && std::abs(y) - std::abs(x) < m_lineWidth) // parte inferior
            ){
                return m_color0 * prev_x;
            }else{
                if(prev_x > 0.5){
                    prev_x -= 0.5;
                }else{
                    prev_x += 0.5;
                }
                return m_color0 * prev_x;
            }
        }
    }

    void evalGradient(const Intersection &its, Spectrum *gradient) const {
        Point2 uv = Point2(its.uv.x * m_uvScale.x, its.uv.y * m_uvScale.y) + m_uvOffset;

        evalGradient(uv, gradient);

        gradient[0] *= m_uvScale.x;
        gradient[1] *= m_uvScale.y;
    }

    void evalGradient(const Point2 &uv, Spectrum *gradient) const {
        const Float eps = Epsilon;

        Spectrum value = eval(uv);
        Spectrum valueU = eval(uv + Vector2(eps, 0));
        Spectrum valueV = eval(uv + Vector2(0, eps));


        gradient[0] = (valueU - value)*(1/eps);
        gradient[1] = (valueV - value)*(1/eps);
    }

    Spectrum eval(const Point2 &uv,
            const Vector2 &d0, const Vector2 &d1) const {
        /* Filtering is currently not supported */
        return reptileSkin::eval(uv);
    }

    bool usesRayDifferentials() const {
        return false;
    }

    Spectrum getMaximum() const {
        Spectrum max;
        for (int i=0; i<SPECTRUM_SAMPLES; ++i)
            max[i] = std::max(m_color0[i], m_color1[i]);
        return max;
    }

    Spectrum getMinimum() const {
        Spectrum min;
        for (int i=0; i<SPECTRUM_SAMPLES; ++i)
            min[i] = std::min(m_color0[i], m_color1[i]);
        return min;
    }

    Spectrum getAverage() const {
        Float interiorWidth = std::max((Float) 0.0f, 1-2*m_lineWidth),
              interiorArea = interiorWidth * interiorWidth,
              lineArea = 1 - interiorArea;
        return m_color1 * lineArea + m_color0 * interiorArea;
    }

    bool isConstant() const {
        return false;
    }

    bool isMonochromatic() const {
        return Spectrum(m_color0[0]) == m_color0
            && Spectrum(m_color1[0]) == m_color1;
    }

    std::string toString() const {
        return "reptileSkin[]";
    }

    Shader *createShader(Renderer *renderer) const;

    MTS_DECLARE_CLASS()
protected:
    Spectrum m_color0;
    Spectrum m_color1;
    Float m_lineWidth;
};

// ================ Hardware shader implementation ================

class reptileSkinShader : public Shader {
public:
    reptileSkinShader(Renderer *renderer, const Spectrum &color0,
        const Spectrum &color1, Float lineWidth, const Point2 &uvOffset,
        const Vector2 &uvScale) : Shader(renderer, ETextureShader),
        m_color0(color0), m_color1(color1),
        m_lineWidth(lineWidth), m_uvOffset(uvOffset), m_uvScale(uvScale) {
    }

    void generateCode(std::ostringstream &oss,
            const std::string &evalName,
            const std::vector<std::string> &depNames) const {
        oss << "uniform vec3 " << evalName << "_color0;" << endl
            << "uniform vec3 " << evalName << "_color1;" << endl
            << "uniform float " << evalName << "_lineWidth;" << endl
            << "uniform vec2 " << evalName << "_uvOffset;" << endl
            << "uniform vec2 " << evalName << "_uvScale;" << endl
            << endl
            << "vec3 " << evalName << "(vec2 uv) {" << endl
            << "    uv = vec2(" << endl
            << "        uv.x * " << evalName << "_uvScale.x + " << evalName << "_uvOffset.x," << endl
            << "        uv.y * " << evalName << "_uvScale.y + " << evalName << "_uvOffset.y);" << endl
            << "    float x = uv.x - floor(uv.x);" << endl
            << "    float y = uv.y - floor(uv.y);" << endl
            << "    if (x > .5) x -= 1.0;" << endl
            << "    if (y > .5) y -= 1.0;" << endl
            << "    if (abs(x) < " << evalName << "_lineWidth || abs(y) < " << evalName << "_lineWidth)" << endl
            << "        return " << evalName << "_color1;" << endl
            << "    else" << endl
            << "        return " << evalName << "_color0;" << endl
            << "}" << endl;
    }

    void resolve(const GPUProgram *program, const std::string &evalName, std::vector<int> &parameterIDs) const {
        parameterIDs.push_back(program->getParameterID(evalName + "_color0", false));
        parameterIDs.push_back(program->getParameterID(evalName + "_color1", false));
        parameterIDs.push_back(program->getParameterID(evalName + "_lineWidth", false));
        parameterIDs.push_back(program->getParameterID(evalName + "_uvOffset", false));
        parameterIDs.push_back(program->getParameterID(evalName + "_uvScale", false));
    }

    void bind(GPUProgram *program, const std::vector<int> &parameterIDs,
        int &textureUnitOffset) const {
        program->setParameter(parameterIDs[0], m_color0);
        program->setParameter(parameterIDs[1], m_color1);
        program->setParameter(parameterIDs[2], m_lineWidth);
        program->setParameter(parameterIDs[3], m_uvOffset);
        program->setParameter(parameterIDs[4], m_uvScale);
    }

    MTS_DECLARE_CLASS()
private:
    Spectrum m_color0;
    Spectrum m_color1;
    Float m_lineWidth;
    Point2 m_uvOffset;
    Vector2 m_uvScale;
};

Shader *reptileSkin::createShader(Renderer *renderer) const {
    return new reptileSkinShader(renderer, m_color0, m_color1,
            m_lineWidth, m_uvOffset, m_uvScale);
}

MTS_IMPLEMENT_CLASS(reptileSkinShader, false, Shader)
MTS_IMPLEMENT_CLASS_S(reptileSkin, false, Texture2D)
MTS_EXPORT_PLUGIN(reptileSkin, "Grid texture");
MTS_NAMESPACE_END

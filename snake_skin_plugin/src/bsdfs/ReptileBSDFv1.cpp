/*
    This file has been created by Diego Bielsa Monterde.

    It is part of his TFG and it has been created based on the file
    diffuse.cpp. 
*/

#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/hw/basicshader.h>
#include <mitsuba/core/warp.h>
#include "ior.h"

MTS_NAMESPACE_BEGIN

/*
 *
 *  This BSDF represents reptile's skin BSDFv1, this is the base implementation of
 *  the TFG
 */
class ReptileBSDFv1 : public BSDF {
public:
    ReptileBSDFv1(const Properties &props) : BSDF(props) {
        /* Specifies the internal index of refraction at the interface */
        Float intIOR = lookupIOR(props, "intIOR", "polypropylene");

        /* Specifies the external index of refraction at the interface */
        Float extIOR = lookupIOR(props, "extIOR", "air");

        if (intIOR < 0 || extIOR < 0)
            Log(EError, "The interior and exterior indices of "
                "refraction must be positive!");

        m_eta = intIOR / extIOR;
        m_invEta = 1 / m_eta;

        /* Specifies the layer's thickness using the inverse units of sigmaA */
        m_thickness = props.getFloat("thickness", 1);

        /* Specifies the absorption within the layer */
        m_sigmaA = new ConstantSpectrumTexture(
            props.getSpectrum("sigmaA", Spectrum(0.0f)));

        m_specularReflectance = new ConstantSpectrumTexture(
            props.getSpectrum("specularReflectance", Spectrum(1.0f)));
        m_diffuseReflectance = new ConstantSpectrumTexture(
            props.getSpectrum("diffuseReflectance", Spectrum(0.5f)));

        m_nonlinear = props.getBoolean("nonlinear", false);

        m_specularSamplingWeight = 0.0f;
    }

    ReptileBSDFv1(Stream *stream, InstanceManager *manager)
            : BSDF(stream, manager) {
        m_eta = stream->readFloat();
        m_thickness = stream->readFloat();
        m_nonlinear = stream->readBool();
        m_sigmaA = static_cast<Texture *>(manager->getInstance(stream));
        m_specularReflectance = static_cast<Texture *>(manager->getInstance(stream));
        m_diffuseReflectance = static_cast<Texture *>(manager->getInstance(stream));
        m_invEta = 1 / m_eta;
        configure();
    }

    void serialize(Stream *stream, InstanceManager *manager) const {
        BSDF::serialize(stream, manager);

        stream->writeFloat(m_eta);
        stream->writeFloat(m_thickness);
        stream->writeBool(m_nonlinear);
        manager->serialize(stream, m_sigmaA.get());
        manager->serialize(stream, m_specularReflectance.get());
        manager->serialize(stream, m_diffuseReflectance.get());
    }

    void configure() {
        /* Verify the input parameters and fix them if necessary */
        m_specularReflectance = ensureEnergyConservation(
            m_specularReflectance, "specularReflectance", 1.0f);
        m_diffuseReflectance = ensureEnergyConservation(
            m_diffuseReflectance, "diffuseReflectance", 1.0f);

        /* Numerically approximate the diffuse Fresnel reflectance */
        m_fdrInt = fresnelDiffuseReflectance(1/m_eta, false);
        m_fdrExt = fresnelDiffuseReflectance(m_eta, false);

        unsigned int extraFlags = 0;
        m_components.clear();
        if (!m_sigmaA->isConstant()){
            extraFlags |= ESpatiallyVarying;
            m_components.push_back(extraFlags);
        }

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float dAvg = m_diffuseReflectance->getAverage().getLuminance(),
              sAvg = m_specularReflectance->getAverage().getLuminance(),
              avgAbsorption = (m_sigmaA->getAverage()*(-2*m_thickness)).exp().average();
            

        m_specularSamplingWeight = sAvg / (dAvg + sAvg + avgAbsorption);

        m_invEta2 = 1/(m_eta*m_eta);

        m_usesRayDifferentials =
            m_specularReflectance->usesRayDifferentials() ||
            m_diffuseReflectance->usesRayDifferentials() || 
            m_sigmaA->usesRayDifferentials();

        
        m_components.push_back(EDeltaReflection | EFrontSide
            | (m_specularReflectance->isConstant() ? 0 : ESpatiallyVarying));
        m_components.push_back(EDiffuseReflection | EFrontSide
            | (m_diffuseReflectance->isConstant() ? 0 : ESpatiallyVarying));

        BSDF::configure();
    }

    Spectrum getDiffuseReflectance(const Intersection &its) const {
        return m_diffuseReflectance->eval(its) * (1-m_fdrExt);
    }

    Spectrum getSpecularReflectance(const Intersection &its) const {
        return m_specularReflectance->eval(its);
    }

    void addChild(const std::string &name, ConfigurableObject *child) {
        if (child->getClass()->derivesFrom(MTS_CLASS(Texture))) {
            if (name == "specularReflectance")
                m_specularReflectance = static_cast<Texture *>(child);
            else if (name == "diffuseReflectance")
                m_diffuseReflectance = static_cast<Texture *>(child);
            else if (name == "sigmaA")
                m_sigmaA = static_cast<Texture *>(child);
            else
                BSDF::addChild(name, child);
        } else {
            BSDF::addChild(name, child);
        }
    }

    /// Reflection in local coordinates
    inline Vector reflect(const Vector &wi) const {
        return Vector(-wi.x, -wi.y, wi.z);
    }

    /// Refract into the material, preserve sign of direction
    inline Vector refractIn(const Vector &wi, Float &R) const {
        Float cosThetaT;
        R = fresnelDielectricExt(std::abs(Frame::cosTheta(wi)), cosThetaT, m_eta);
        return Vector(m_invEta*wi.x, m_invEta*wi.y, -math::signum(Frame::cosTheta(wi)) * cosThetaT);
    }

    /// Refract out of the material, preserve sign of direction
    inline Vector refractOut(const Vector &wi, Float &R) const {
        Float cosThetaT;
        R = fresnelDielectricExt(std::abs(Frame::cosTheta(wi)), cosThetaT, m_invEta);
        return Vector(m_eta*wi.x, m_eta*wi.y, -math::signum(Frame::cosTheta(wi)) * cosThetaT);
    }

    Spectrum eval(const BSDFSamplingRecord &bRec, EMeasure measure) const {
        bool hasSpecular   = (bRec.typeMask & EDeltaReflection)
                && (bRec.component == -1 || bRec.component == 0)
                && measure == EDiscrete;
        bool hasDiffuse = (bRec.typeMask & EDiffuseReflection)
                && (bRec.component == -1 || bRec.component == 1)
                && measure == ESolidAngle;

        if (Frame::cosTheta(bRec.wo) <= 0 || Frame::cosTheta(bRec.wi) <= 0)
            return Spectrum(0.0f);

        Float Fi = fresnelDielectricExt(Frame::cosTheta(bRec.wi), m_eta);

        if (hasSpecular) {
            /* Check if the provided direction pair matches an ideal
               specular reflection; tolerate some roundoff errors */
            if (std::abs(dot(reflect(bRec.wi), bRec.wo)-1) < DeltaEpsilon)
                return m_specularReflectance->eval(bRec.its) * Fi;
        } else if (hasDiffuse) {
            Float Fo = fresnelDielectricExt(Frame::cosTheta(bRec.wo), m_eta);

            Spectrum diff = m_diffuseReflectance->eval(bRec.its);

            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff * m_fdrInt;
            else
                diff /= 1 - m_fdrInt;

            /* Applying absorption */
            Spectrum sigmaA = m_sigmaA->eval(bRec.its) * m_thickness;
            if (!sigmaA.isZero())
                diff *= (-sigmaA *
                    (1/std::abs(Frame::cosTheta(bRec.wi)) +
                     1/std::abs(Frame::cosTheta(bRec.wo)))).exp();

            return diff * (warp::squareToCosineHemispherePdf(bRec.wo)
                * m_invEta2 * (1-Fi) * (1-Fo));
        }

        return Spectrum(0.0f);
    }

    Float pdf(const BSDFSamplingRecord &bRec, EMeasure measure) const {
        bool hasSpecular   = (bRec.typeMask & EDeltaReflection)
                && (bRec.component == -1 || bRec.component == 0);
        bool hasDiffuse = (bRec.typeMask & EDiffuseReflection)
                && (bRec.component == -1 || bRec.component == 1);

        if (Frame::cosTheta(bRec.wo) <= 0 || Frame::cosTheta(bRec.wi) <= 0)
            return 0.0f;

        Float probSpecular = hasSpecular ? 1.0f : 0.0f;
        if (hasSpecular && hasDiffuse) {
            Float Fi = fresnelDielectricExt(Frame::cosTheta(bRec.wi), m_eta);
            probSpecular = (Fi*m_specularSamplingWeight) /
                (Fi*m_specularSamplingWeight +
                (1-Fi) * (1-m_specularSamplingWeight));
        }

        if (hasSpecular && measure == EDiscrete) {
            /* Check if the provided direction pair matches an ideal
               specular reflection; tolerate some roundoff errors */
            if (std::abs(dot(reflect(bRec.wi), bRec.wo)-1) < DeltaEpsilon)
                return probSpecular;
        } else if (hasDiffuse && measure == ESolidAngle) {
            return warp::squareToCosineHemispherePdf(bRec.wo) * (1-probSpecular);
        }

        return 0.0f;
    }

    Spectrum sample(BSDFSamplingRecord &bRec, const Point2 &sample) const {
        bool hasSpecular   = (bRec.typeMask & EDeltaReflection)
                && (bRec.component == -1 || bRec.component == 0);
        bool hasDiffuse = (bRec.typeMask & EDiffuseReflection)
                && (bRec.component == -1 || bRec.component == 1);

        if ((!hasDiffuse && !hasSpecular) || Frame::cosTheta(bRec.wi) <= 0)
            return Spectrum(0.0f);

        Float Fi = fresnelDielectricExt(Frame::cosTheta(bRec.wi), m_eta);

        bRec.eta = 1.0f;
        if (hasDiffuse && hasSpecular) {
            Float probSpecular = (Fi*m_specularSamplingWeight) /
                (Fi*m_specularSamplingWeight +
                (1-Fi) * (1-m_specularSamplingWeight));

            /* Importance sample wrt. the Fresnel reflectance */
            if (sample.x < probSpecular) {
                bRec.sampledComponent = 0;
                bRec.sampledType = EDeltaReflection;
                bRec.wo = reflect(bRec.wi);

                return m_specularReflectance->eval(bRec.its)
                    * Fi / probSpecular;
            } else {
                bRec.sampledComponent = 1;
                bRec.sampledType = EDiffuseReflection;
                bRec.wo = warp::squareToCosineHemisphere(Point2(
                    (sample.x - probSpecular) / (1 - probSpecular),
                    sample.y
                ));
                Float Fo = fresnelDielectricExt(Frame::cosTheta(bRec.wo), m_eta);

               

                Spectrum diff = m_diffuseReflectance->eval(bRec.its);

                 /* Applying absorption */
                Spectrum sigmaA = m_sigmaA->eval(bRec.its) * m_thickness;
                if (!sigmaA.isZero())
                    diff *= (-sigmaA *
                        (1/std::abs(Frame::cosTheta(bRec.wi)) +
                        1/std::abs(Frame::cosTheta(bRec.wo)))).exp();

                if (m_nonlinear)
                    diff /= Spectrum(1.0f) - diff*m_fdrInt;
                else
                    diff /= 1 - m_fdrInt;

                return diff * (m_invEta2 * (1-Fi) * (1-Fo) / (1-probSpecular));
            }
        } else if (hasSpecular) {
            bRec.sampledComponent = 0;
            bRec.sampledType = EDeltaReflection;
            bRec.wo = reflect(bRec.wi);
            return m_specularReflectance->eval(bRec.its) * Fi;
        } else {
            bRec.sampledComponent = 1;
            bRec.sampledType = EDiffuseReflection;
            bRec.wo = warp::squareToCosineHemisphere(sample);
            Float Fo = fresnelDielectricExt(Frame::cosTheta(bRec.wo), m_eta);

            Spectrum diff = m_diffuseReflectance->eval(bRec.its);
            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff*m_fdrInt;
            else
                diff /= 1 - m_fdrInt;

            return diff * (m_invEta2 * (1-Fi) * (1-Fo));
        }
    }

    Spectrum sample(BSDFSamplingRecord &bRec, Float &pdf, const Point2 &sample) const {
        bool hasSpecular   = (bRec.typeMask & EDeltaReflection)
                && (bRec.component == -1 || bRec.component == 0);
        bool hasDiffuse = (bRec.typeMask & EDiffuseReflection)
                && (bRec.component == -1 || bRec.component == 1);

        if ((!hasDiffuse && !hasSpecular) || Frame::cosTheta(bRec.wi) <= 0)
            return Spectrum(0.0f);

        Float Fi = fresnelDielectricExt(Frame::cosTheta(bRec.wi), m_eta);

        bRec.eta = 1.0f;
        if (hasDiffuse && hasSpecular) {
            Float probSpecular = (Fi*m_specularSamplingWeight) /
                (Fi*m_specularSamplingWeight +
                (1-Fi) * (1-m_specularSamplingWeight));

            /* Importance sample wrt. the Fresnel reflectance */
            if (sample.x < probSpecular) {
                bRec.sampledComponent = 0;
                bRec.sampledType = EDeltaReflection;
                bRec.wo = reflect(bRec.wi);

                pdf = probSpecular;
                return m_specularReflectance->eval(bRec.its)
                    * Fi / probSpecular;
            } else {
                bRec.sampledComponent = 1;
                bRec.sampledType = EDiffuseReflection;
                bRec.wo = warp::squareToCosineHemisphere(Point2(
                    (sample.x - probSpecular) / (1 - probSpecular),
                    sample.y
                ));
                Float Fo = fresnelDielectricExt(Frame::cosTheta(bRec.wo), m_eta);

                Spectrum diff = m_diffuseReflectance->eval(bRec.its);
                
                /* Applying absorption */
                Spectrum sigmaA = m_sigmaA->eval(bRec.its) * m_thickness;
                if (!sigmaA.isZero())
                    diff *= (-sigmaA *
                        (1/std::abs(Frame::cosTheta(bRec.wi)) +
                        1/std::abs(Frame::cosTheta(bRec.wo)))).exp();

                if (m_nonlinear)
                    diff /= Spectrum(1.0f) - diff*m_fdrInt;
                else
                    diff /= 1 - m_fdrInt;

                pdf = (1-probSpecular) *
                    warp::squareToCosineHemispherePdf(bRec.wo);

                return diff * (m_invEta2 * (1-Fi) * (1-Fo) / (1-probSpecular));
            }
        } else if (hasSpecular) {
            bRec.sampledComponent = 0;
            bRec.sampledType = EDeltaReflection;
            bRec.wo = reflect(bRec.wi);
            pdf = 1;
            return m_specularReflectance->eval(bRec.its) * Fi;
        } else {
            bRec.sampledComponent = 1;
            bRec.sampledType = EDiffuseReflection;
            bRec.wo = warp::squareToCosineHemisphere(sample);
            Float Fo = fresnelDielectricExt(Frame::cosTheta(bRec.wo), m_eta);

            Spectrum diff = m_diffuseReflectance->eval(bRec.its);
            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff*m_fdrInt;
            else
                diff /= 1 - m_fdrInt;

            pdf = warp::squareToCosineHemispherePdf(bRec.wo);

            return diff * (m_invEta2 * (1-Fi) * (1-Fo));
        }
    }

    Float getRoughness(const Intersection &its, int component) const {
        Assert(component == 0 || component == 1);

        if (component == 0)
            return 0.0f;
        else
            return std::numeric_limits<Float>::infinity();
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "ReptileBSDFv1[" << endl
            << "  id = \"" << getID() << "\"," << endl
            << "  specularReflectance = " << indent(m_specularReflectance->toString()) << "," << endl
            << "  diffuseReflectance = " << indent(m_diffuseReflectance->toString()) << "," << endl
            << "  specularSamplingWeight = " << m_specularSamplingWeight << "," << endl
            << "  diffuseSamplingWeight = " << (1-m_specularSamplingWeight) << "," << endl
            << "  nonlinear = " << m_nonlinear << "," << endl
            << "  eta = " << m_eta << "," << endl
            << "  fdrInt = " << m_fdrInt << "," << endl
            << "  fdrExt = " << m_fdrExt << endl
            << "]";
        return oss.str();
    }


    MTS_DECLARE_CLASS()
protected:
    Float m_fdrInt, m_fdrExt, m_eta, m_invEta2, m_invEta;
    ref<Texture> m_diffuseReflectance;
    ref<Texture> m_specularReflectance;
    ref<Texture> m_sigmaA;
    Float m_specularSamplingWeight;
    bool m_nonlinear;
    Float m_thickness;
};


MTS_IMPLEMENT_CLASS_S(ReptileBSDFv1, false, BSDF)
MTS_EXPORT_PLUGIN(ReptileBSDFv1, "Reptile BSDFv1")
MTS_NAMESPACE_END

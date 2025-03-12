/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2014 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/render/bsdf.h>
#include <mitsuba/hw/basicshader.h>
#include "ior.h"

MTS_NAMESPACE_BEGIN

/*! \plugin{coating}{Smooth dielectric coating}
 * \order{10}
 * \icon{bsdf_coating}
 *
 * \parameters{
 *     \parameter{intIOR}{\Float\Or\String}{Interior index of refraction specified
 *      numerically or using a known material name. \default{\texttt{bk7} / 1.5046}}
 *     \parameter{extIOR}{\Float\Or\String}{Exterior index of refraction specified
 *      numerically or using a known material name. \default{\texttt{air} / 1.000277}}
 *     \parameter{thickness}{\Float}{Denotes the thickness of the layer (to
 *      model absorption --- should be specified in inverse units of \code{sigmaA})\default{1}}
 *     \parameter{sigmaA}{\Spectrum\Or\Texture}{The absorption coefficient of the
 *      coating layer. \default{0, i.e. there is no absorption}}
 *     \parameter{specular\showbreak Reflectance}{\Spectrum\Or\Texture}{Optional
 *         factor that can be used to modulate the specular reflection component. Note
 *         that for physical realism, this parameter should never be touched. \default{1.0}}
 *     \parameter{\Unnamed}{\BSDF}{A nested BSDF model that should be coated.}
 * }
 *
 * \renderings{
 *     \rendering{Rough copper}
 *         {bsdf_coating_uncoated}
 *     \rendering{The same material coated with a single layer of
 *         clear varnish (see \lstref{coating-roughcopper})}
 *         {bsdf_coating_roughconductor}
 * }
 *
 * This plugin implements a smooth dielectric coating (e.g. a layer of varnish)
 * in the style of the paper ``Arbitrarily Layered Micro-Facet Surfaces'' by
 * Weidlich and Wilkie \cite{Weidlich2007Arbitrarily}. Any BSDF in Mitsuba
 * can be coated using this plugin, and multiple coating layers can even
 * be applied in sequence. This allows designing interesting custom materials
 * like car paint or glazed metal foil. The coating layer can optionally be
 * tinted (i.e. filled with an absorbing medium), in which case this model also
 * accounts for the directionally dependent absorption within the layer.
 *
 * Note that the plugin discards illumination that undergoes internal
 * reflection within the coating. This can lead to a noticeable energy
 * loss for materials that reflect much of their energy near or below the critical
 * angle (i.e. diffuse or very rough materials).
 * Therefore, users are discouraged to use this plugin to coat smooth
 * diffuse materials, since there is a separately available plugin
 * named \pluginref{plastic}, which covers the same case and does not
 * suffer from energy loss.\newpage
 *
 * \renderings{
 *     \smallrendering{$\code{thickness}=0$}{bsdf_coating_0}
 *     \smallrendering{$\code{thickness}=1$}{bsdf_coating_1}
 *     \smallrendering{$\code{thickness}=5$}{bsdf_coating_5}
 *     \smallrendering{$\code{thickness}=15$}{bsdf_coating_15}
 *     \caption{The effect of the layer thickness parameter on
 *        a tinted coating ($\code{sigmaT}=(0.1, 0.2, 0.5)$)}
 * }
 *
 * \vspace{4mm}
 *
 * \begin{xml}[caption=Rough copper coated with a transparent layer of
 *     varnish, label=lst:coating-roughcopper]
 * <bsdf type="coating">
 *     <float name="intIOR" value="1.7"/>
 *
 *     <bsdf type="roughconductor">
 *         <string name="material" value="Cu"/>
 *         <float name="alpha" value="0.1"/>
 *     </bsdf>
 * </bsdf>
 * \end{xml}
 *
 * \renderings{
 *     \rendering{Coated rough copper with a bump map applied on top}{bsdf_coating_coatedbump}
 *     \rendering{Bump mapped rough copper with a coating on top}{bsdf_coating_bumpcoating}
 *     \caption{Some interesting materials can be created simply by applying
 *     Mitsuba's material modifiers in different orders.}
 * }
 *
 * \subsubsection*{Technical details}
 * Evaluating the internal component of this model entails refracting the
 * incident and exitant rays through the dielectric interface, followed by
 * querying the nested material with this modified direction pair. The result
 * is attenuated by the two Fresnel transmittances and the absorption, if
 * any.
 */
class Absorption : public BSDF {
public:
    Absorption(const Properties &props)
            : BSDF(props) {


        /* Specifies the layer's thickness using the inverse units of sigmaA */
        m_thickness = props.getFloat("thickness", 1);

        /* Specifies the absorption within the layer */
        m_sigmaA = new ConstantSpectrumTexture(
            props.getSpectrum("sigmaA", Spectrum(0.0f)));
    }

    Absorption(Stream *stream, InstanceManager *manager)
            : BSDF(stream, manager) {
        m_thickness = stream->readFloat();
        m_sigmaA = static_cast<Texture *>(manager->getInstance(stream));
        configure();
    }

    void serialize(Stream *stream, InstanceManager *manager) const {
        BSDF::serialize(stream, manager);

        stream->writeFloat(m_thickness);
        manager->serialize(stream, m_sigmaA.get());
    }

    void configure() {

        unsigned int extraFlags = 0;
        if (!m_sigmaA->isConstant())
            extraFlags |= ESpatiallyVarying;

        m_components.clear();

        m_components.push_back(EFrontSide | EBackSide);

        m_usesRayDifferentials = m_sigmaA->usesRayDifferentials();

        /* Compute weights that further steer samples towards
           the specular or nested components */
        Float avgAbsorption = (m_sigmaA->getAverage()
             *(-2*m_thickness)).exp().average();


        BSDF::configure();
    }

    void addChild(const std::string &name, ConfigurableObject *child) {
        if (child->getClass()->derivesFrom(MTS_CLASS(Texture)) && name == "sigmaA") {
            m_sigmaA = static_cast<Texture *>(child);
        } else {
            BSDF::addChild(name, child);
        }
    }

     /// Refraction in local coordinates
    inline Vector refract(const Vector &wi) const {
        Float cosThetaT = -Frame::cosTheta(wi);
        Float scale = -(cosThetaT < 0 ? 1 : 1);
        return Vector(scale*wi.x, scale*wi.y, cosThetaT);
    }


    Spectrum eval(const BSDFSamplingRecord &bRec, EMeasure measure) const {
        // wi y wo es el mismo
        Spectrum result = Spectrum(1.0f);
        Vector bRecWo = refract(bRec.wi);
        Spectrum sigmaA = m_sigmaA->eval(bRec.its) * m_thickness;
        if (!sigmaA.isZero())
            result *= (-sigmaA *
                (1/std::abs(Frame::cosTheta(bRec.wi)) +
                    1/std::abs(Frame::cosTheta(bRecWo)))).exp();

        return result;
        

    }

    Float pdf(const BSDFSamplingRecord &bRec, EMeasure measure) const {
        return 1.0f;
    }

    Spectrum sample(BSDFSamplingRecord &bRec, Float &pdf, const Point2 &_sample) const {
        bRec.wo = refract(bRec.wi);; // no cambia
        pdf = 1.0f;

        Spectrum result = Spectrum(1.0f);

        Spectrum sigmaA = m_sigmaA->eval(bRec.its) * m_thickness;
        if (!sigmaA.isZero())
            result *= (-sigmaA *
                (1/std::abs(Frame::cosTheta(bRec.wi)) +
                    1/std::abs(Frame::cosTheta(bRec.wo)))).exp();


        return result;
        
    }

    Spectrum sample(BSDFSamplingRecord &bRec, const Point2 &sample) const {
        Float pdf;
        return Absorption::sample(bRec, pdf, sample);
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "Absorption[" << endl
            << "  sigmaA = " << indent(m_sigmaA->toString()) << "," << endl
            << "  thickness = " << m_thickness << "," << endl
            << "]";
        return oss.str();
    }


    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_sigmaA;
    Float m_thickness;
};



MTS_IMPLEMENT_CLASS_S(Absorption, false, BSDF)
MTS_EXPORT_PLUGIN(Absorption, "Absorption");
MTS_NAMESPACE_END

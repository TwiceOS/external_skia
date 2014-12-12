/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrBezierEffect.h"

#include "gl/GrGLProcessor.h"
#include "gl/GrGLSL.h"
#include "gl/GrGLGeometryProcessor.h"
#include "gl/builders/GrGLProgramBuilder.h"

class GrGLConicEffect : public GrGLGeometryProcessor {
public:
    GrGLConicEffect(const GrGeometryProcessor&,
                    const GrBatchTracker&);

    virtual void emitCode(const EmitArgs&) SK_OVERRIDE;

    static inline void GenKey(const GrGeometryProcessor&,
                              const GrBatchTracker&,
                              const GrGLCaps&,
                              GrProcessorKeyBuilder*);

    virtual void setData(const GrGLProgramDataManager&,
                         const GrGeometryProcessor&,
                         const GrBatchTracker&) SK_OVERRIDE {}

private:
    GrPrimitiveEdgeType fEdgeType;

    typedef GrGLGeometryProcessor INHERITED;
};

GrGLConicEffect::GrGLConicEffect(const GrGeometryProcessor& processor,
                                 const GrBatchTracker& bt) {
    const GrConicEffect& ce = processor.cast<GrConicEffect>();
    fEdgeType = ce.getEdgeType();
}

void GrGLConicEffect::emitCode(const EmitArgs& args) {
    GrGLVertexBuilder* vsBuilder = args.fPB->getVertexShaderBuilder();
    const GrConicEffect& gp = args.fGP.cast<GrConicEffect>();

    GrGLVertToFrag v(kVec4f_GrSLType);
    args.fPB->addVarying("ConicCoeffs", &v);
    vsBuilder->codeAppendf("%s = %s;", v.vsOut(), gp.inConicCoeffs()->fName);

    // setup coord outputs
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->positionCoords(), gp.inPosition()->fName);
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->localCoords(), gp.inPosition()->fName);

    // setup position varying
    vsBuilder->codeAppendf("%s = %s * vec3(%s, 1);", vsBuilder->glPosition(), vsBuilder->uViewM(),
                           gp.inPosition()->fName);

    GrGLGPFragmentBuilder* fsBuilder = args.fPB->getFragmentShaderBuilder();
    fsBuilder->codeAppend("float edgeAlpha;");

    switch (fEdgeType) {
        case kHairlineAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("vec3 dklmdx = dFdx(%s.xyz);", v.fsIn());
            fsBuilder->codeAppendf("vec3 dklmdy = dFdy(%s.xyz);", v.fsIn());
            fsBuilder->codeAppendf("float dfdx ="
                                   "2.0 * %s.x * dklmdx.x - %s.y * dklmdx.z - %s.z * dklmdx.y;",
                                   v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("float dfdy ="
                                   "2.0 * %s.x * dklmdy.x - %s.y * dklmdy.z - %s.z * dklmdy.y;",
                                   v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("vec2 gF = vec2(dfdx, dfdy);");
            fsBuilder->codeAppend("float gFM = sqrt(dot(gF, gF));");
            fsBuilder->codeAppendf("float func = %s.x*%s.x - %s.y*%s.z;", v.fsIn(), v.fsIn(),
                                   v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("func = abs(func);");
            fsBuilder->codeAppend("edgeAlpha = func / gFM;");
            fsBuilder->codeAppend("edgeAlpha = max(1.0 - edgeAlpha, 0.0);");
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");
            break;
        }
        case kFillAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("vec3 dklmdx = dFdx(%s.xyz);", v.fsIn());
            fsBuilder->codeAppendf("vec3 dklmdy = dFdy(%s.xyz);", v.fsIn());
            fsBuilder->codeAppendf("float dfdx ="
                                   "2.0 * %s.x * dklmdx.x - %s.y * dklmdx.z - %s.z * dklmdx.y;",
                                   v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("float dfdy ="
                                   "2.0 * %s.x * dklmdy.x - %s.y * dklmdy.z - %s.z * dklmdy.y;",
                                   v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("vec2 gF = vec2(dfdx, dfdy);");
            fsBuilder->codeAppend("float gFM = sqrt(dot(gF, gF));");
            fsBuilder->codeAppendf("float func = %s.x * %s.x - %s.y * %s.z;", v.fsIn(), v.fsIn(),
                                   v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("edgeAlpha = func / gFM;");
            fsBuilder->codeAppend("edgeAlpha = clamp(1.0 - edgeAlpha, 0.0, 1.0);");
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");
            break;
        }
        case kFillBW_GrProcessorEdgeType: {
            fsBuilder->codeAppendf("edgeAlpha = %s.x * %s.x - %s.y * %s.z;", v.fsIn(), v.fsIn(),
                                   v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("edgeAlpha = float(edgeAlpha < 0.0);");
            break;
        }
        default:
            SkFAIL("Shouldn't get here");
    }

    fsBuilder->codeAppendf("%s = vec4(edgeAlpha);", args.fOutputCoverage);
}

void GrGLConicEffect::GenKey(const GrGeometryProcessor& processor,
                             const GrBatchTracker&,
                             const GrGLCaps&,
                             GrProcessorKeyBuilder* b) {
    const GrConicEffect& ce = processor.cast<GrConicEffect>();
    uint32_t key = ce.isAntiAliased() ? (ce.isFilled() ? 0x0 : 0x1) : 0x2;
    b->add32(key);
}

//////////////////////////////////////////////////////////////////////////////

GrConicEffect::~GrConicEffect() {}

void GrConicEffect::getGLProcessorKey(const GrBatchTracker& bt,
                                      const GrGLCaps& caps,
                                      GrProcessorKeyBuilder* b) const {
    GrGLConicEffect::GenKey(*this, bt, caps, b);
}

GrGLGeometryProcessor* GrConicEffect::createGLInstance(const GrBatchTracker& bt) const {
    return SkNEW_ARGS(GrGLConicEffect, (*this, bt));
}

GrConicEffect::GrConicEffect(GrColor color, uint8_t coverage, GrPrimitiveEdgeType edgeType)
    : INHERITED(color, false, coverage), fEdgeType(edgeType) {
    this->initClassID<GrConicEffect>();
    fInPosition = &this->addVertexAttrib(GrAttribute("inPosition", kVec2f_GrVertexAttribType));
    fInConicCoeffs = &this->addVertexAttrib(GrAttribute("inConicCoeffs",
                                                        kVec4f_GrVertexAttribType));
}

bool GrConicEffect::onIsEqual(const GrGeometryProcessor& other) const {
    const GrConicEffect& ce = other.cast<GrConicEffect>();
    return (ce.fEdgeType == fEdgeType);
}

//////////////////////////////////////////////////////////////////////////////

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(GrConicEffect);

GrGeometryProcessor* GrConicEffect::TestCreate(SkRandom* random,
                                               GrContext*,
                                               const GrDrawTargetCaps& caps,
                                               GrTexture*[]) {
    GrGeometryProcessor* gp;
    do {
        GrPrimitiveEdgeType edgeType = static_cast<GrPrimitiveEdgeType>(
                                                    random->nextULessThan(kGrProcessorEdgeTypeCnt));
        gp = GrConicEffect::Create(GrRandomColor(random), edgeType, caps);
    } while (NULL == gp);
    return gp;
}

//////////////////////////////////////////////////////////////////////////////
// Quad
//////////////////////////////////////////////////////////////////////////////

class GrGLQuadEffect : public GrGLGeometryProcessor {
public:
    GrGLQuadEffect(const GrGeometryProcessor&,
                   const GrBatchTracker&);

    virtual void emitCode(const EmitArgs&) SK_OVERRIDE;

    static inline void GenKey(const GrGeometryProcessor&,
                              const GrBatchTracker&,
                              const GrGLCaps&,
                              GrProcessorKeyBuilder*);

    virtual void setData(const GrGLProgramDataManager&,
                         const GrGeometryProcessor&,
                         const GrBatchTracker&) SK_OVERRIDE {}

private:
    GrPrimitiveEdgeType fEdgeType;

    typedef GrGLGeometryProcessor INHERITED;
};

GrGLQuadEffect::GrGLQuadEffect(const GrGeometryProcessor& processor,
                               const GrBatchTracker& bt) {
    const GrQuadEffect& ce = processor.cast<GrQuadEffect>();
    fEdgeType = ce.getEdgeType();
}

void GrGLQuadEffect::emitCode(const EmitArgs& args) {
    GrGLVertexBuilder* vsBuilder = args.fPB->getVertexShaderBuilder();
    const GrQuadEffect& gp = args.fGP.cast<GrQuadEffect>();

    GrGLVertToFrag v(kVec4f_GrSLType);
    args.fPB->addVarying("HairQuadEdge", &v);
    vsBuilder->codeAppendf("%s = %s;", v.vsOut(), gp.inHairQuadEdge()->fName);

    // setup coord outputs
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->positionCoords(), gp.inPosition()->fName);
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->localCoords(), gp.inPosition()->fName);

    // setup position varying
    vsBuilder->codeAppendf("%s = %s * vec3(%s, 1);", vsBuilder->glPosition(), vsBuilder->uViewM(),
                           gp.inPosition()->fName);

    GrGLGPFragmentBuilder* fsBuilder = args.fPB->getFragmentShaderBuilder();
    fsBuilder->codeAppendf("float edgeAlpha;");

    switch (fEdgeType) {
        case kHairlineAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("vec2 duvdx = dFdx(%s.xy);", v.fsIn());
            fsBuilder->codeAppendf("vec2 duvdy = dFdy(%s.xy);", v.fsIn());
            fsBuilder->codeAppendf("vec2 gF = vec2(2.0 * %s.x * duvdx.x - duvdx.y,"
                                   "               2.0 * %s.x * duvdy.x - duvdy.y);",
                                   v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("edgeAlpha = (%s.x * %s.x - %s.y);", v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("edgeAlpha = sqrt(edgeAlpha * edgeAlpha / dot(gF, gF));");
            fsBuilder->codeAppend("edgeAlpha = max(1.0 - edgeAlpha, 0.0);");
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");
            break;
        }
        case kFillAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("vec2 duvdx = dFdx(%s.xy);", v.fsIn());
            fsBuilder->codeAppendf("vec2 duvdy = dFdy(%s.xy);", v.fsIn());
            fsBuilder->codeAppendf("vec2 gF = vec2(2.0 * %s.x * duvdx.x - duvdx.y,"
                                   "               2.0 * %s.x * duvdy.x - duvdy.y);",
                                   v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("edgeAlpha = (%s.x * %s.x - %s.y);", v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("edgeAlpha = edgeAlpha / sqrt(dot(gF, gF));");
            fsBuilder->codeAppend("edgeAlpha = clamp(1.0 - edgeAlpha, 0.0, 1.0);");
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");
            break;
        }
        case kFillBW_GrProcessorEdgeType: {
            fsBuilder->codeAppendf("edgeAlpha = (%s.x * %s.x - %s.y);", v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppend("edgeAlpha = float(edgeAlpha < 0.0);");
            break;
        }
        default:
            SkFAIL("Shouldn't get here");
    }

    fsBuilder->codeAppendf("%s = vec4(edgeAlpha);", args.fOutputCoverage);
}

void GrGLQuadEffect::GenKey(const GrGeometryProcessor& processor,
                            const GrBatchTracker&,
                            const GrGLCaps&,
                            GrProcessorKeyBuilder* b) {
    const GrQuadEffect& ce = processor.cast<GrQuadEffect>();
    uint32_t key = ce.isAntiAliased() ? (ce.isFilled() ? 0x0 : 0x1) : 0x2;
    b->add32(key);
}

//////////////////////////////////////////////////////////////////////////////

GrQuadEffect::~GrQuadEffect() {}

void GrQuadEffect::getGLProcessorKey(const GrBatchTracker& bt,
                                     const GrGLCaps& caps,
                                     GrProcessorKeyBuilder* b) const {
    GrGLQuadEffect::GenKey(*this, bt, caps, b);
}

GrGLGeometryProcessor* GrQuadEffect::createGLInstance(const GrBatchTracker& bt) const {
    return SkNEW_ARGS(GrGLQuadEffect, (*this, bt));
}

GrQuadEffect::GrQuadEffect(GrColor color, uint8_t coverage, GrPrimitiveEdgeType edgeType)
    : INHERITED(color, false, coverage), fEdgeType(edgeType) {
    this->initClassID<GrQuadEffect>();
    fInPosition = &this->addVertexAttrib(GrAttribute("inPosition", kVec2f_GrVertexAttribType));
    fInHairQuadEdge = &this->addVertexAttrib(GrAttribute("inHairQuadEdge",
                                                        kVec4f_GrVertexAttribType));
}

bool GrQuadEffect::onIsEqual(const GrGeometryProcessor& other) const {
    const GrQuadEffect& ce = other.cast<GrQuadEffect>();
    return (ce.fEdgeType == fEdgeType);
}

//////////////////////////////////////////////////////////////////////////////

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(GrQuadEffect);

GrGeometryProcessor* GrQuadEffect::TestCreate(SkRandom* random,
                                              GrContext*,
                                              const GrDrawTargetCaps& caps,
                                              GrTexture*[]) {
    GrGeometryProcessor* gp;
    do {
        GrPrimitiveEdgeType edgeType = static_cast<GrPrimitiveEdgeType>(
                random->nextULessThan(kGrProcessorEdgeTypeCnt));
        gp = GrQuadEffect::Create(GrRandomColor(random), edgeType, caps);
    } while (NULL == gp);
    return gp;
}

//////////////////////////////////////////////////////////////////////////////
// Cubic
//////////////////////////////////////////////////////////////////////////////

class GrGLCubicEffect : public GrGLGeometryProcessor {
public:
    GrGLCubicEffect(const GrGeometryProcessor&,
                    const GrBatchTracker&);

    virtual void emitCode(const EmitArgs&) SK_OVERRIDE;

    static inline void GenKey(const GrGeometryProcessor&,
                              const GrBatchTracker&,
                              const GrGLCaps&,
                              GrProcessorKeyBuilder*);

    virtual void setData(const GrGLProgramDataManager&,
                         const GrGeometryProcessor&,
                         const GrBatchTracker&) SK_OVERRIDE {}

private:
    GrPrimitiveEdgeType fEdgeType;

    typedef GrGLGeometryProcessor INHERITED;
};

GrGLCubicEffect::GrGLCubicEffect(const GrGeometryProcessor& processor,
                                 const GrBatchTracker&) {
    const GrCubicEffect& ce = processor.cast<GrCubicEffect>();
    fEdgeType = ce.getEdgeType();
}

void GrGLCubicEffect::emitCode(const EmitArgs& args) {
    GrGLVertexBuilder* vsBuilder = args.fPB->getVertexShaderBuilder();
    const GrCubicEffect& gp = args.fGP.cast<GrCubicEffect>();

    GrGLVertToFrag v(kVec4f_GrSLType);
    args.fPB->addVarying("CubicCoeffs", &v, kHigh_GrSLPrecision);
    vsBuilder->codeAppendf("%s = %s;", v.vsOut(), gp.inCubicCoeffs()->fName);

    // setup coord outputs
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->positionCoords(), gp.inPosition()->fName);
    vsBuilder->codeAppendf("%s = %s;", vsBuilder->localCoords(), gp.inPosition()->fName);

    // setup position varying
    vsBuilder->codeAppendf("%s = %s * vec3(%s, 1);", vsBuilder->glPosition(), vsBuilder->uViewM(),
                           gp.inPosition()->fName);

    GrGLGPFragmentBuilder* fsBuilder = args.fPB->getFragmentShaderBuilder();

    GrGLShaderVar edgeAlpha("edgeAlpha", kFloat_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar dklmdx("dklmdx", kVec3f_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar dklmdy("dklmdy", kVec3f_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar dfdx("dfdx", kFloat_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar dfdy("dfdy", kFloat_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar gF("gF", kVec2f_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar gFM("gFM", kFloat_GrSLType, 0, kHigh_GrSLPrecision);
    GrGLShaderVar func("func", kFloat_GrSLType, 0, kHigh_GrSLPrecision);

    fsBuilder->declAppend(edgeAlpha);
    fsBuilder->declAppend(dklmdx);
    fsBuilder->declAppend(dklmdy);
    fsBuilder->declAppend(dfdx);
    fsBuilder->declAppend(dfdy);
    fsBuilder->declAppend(gF);
    fsBuilder->declAppend(gFM);
    fsBuilder->declAppend(func);

    switch (fEdgeType) {
        case kHairlineAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("%s = dFdx(%s.xyz);", dklmdx.c_str(), v.fsIn());
            fsBuilder->codeAppendf("%s = dFdy(%s.xyz);", dklmdy.c_str(), v.fsIn());
            fsBuilder->codeAppendf("%s = 3.0 * %s.x * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                                   dfdx.c_str(), v.fsIn(), v.fsIn(), dklmdx.c_str(), v.fsIn(),
                                   dklmdx.c_str(), v.fsIn(), dklmdx.c_str());
            fsBuilder->codeAppendf("%s = 3.0 * %s.x * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                                   dfdy.c_str(), v.fsIn(), v.fsIn(), dklmdy.c_str(), v.fsIn(),
                                   dklmdy.c_str(), v.fsIn(), dklmdy.c_str());
            fsBuilder->codeAppendf("%s = vec2(%s, %s);", gF.c_str(), dfdx.c_str(), dfdy.c_str());
            fsBuilder->codeAppendf("%s = sqrt(dot(%s, %s));", gFM.c_str(), gF.c_str(), gF.c_str());
            fsBuilder->codeAppendf("%s = %s.x * %s.x * %s.x - %s.y * %s.z;",
                                   func.c_str(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("%s = abs(%s);", func.c_str(), func.c_str());
            fsBuilder->codeAppendf("%s = %s / %s;",
                                   edgeAlpha.c_str(), func.c_str(), gFM.c_str());
            fsBuilder->codeAppendf("%s = max(1.0 - %s, 0.0);",
                                   edgeAlpha.c_str(), edgeAlpha.c_str());
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppendf("%s = %s * %s * (3.0 - 2.0 * %s);",
            //                        edgeAlpha.c_str(), edgeAlpha.c_str(), edgeAlpha.c_str(),
            //                        edgeAlpha.c_str());
            break;
        }
        case kFillAA_GrProcessorEdgeType: {
            SkAssertResult(fsBuilder->enableFeature(
                    GrGLFragmentShaderBuilder::kStandardDerivatives_GLSLFeature));
            fsBuilder->codeAppendf("%s = dFdx(%s.xyz);", dklmdx.c_str(), v.fsIn());
            fsBuilder->codeAppendf("%s = dFdy(%s.xyz);", dklmdy.c_str(), v.fsIn());
            fsBuilder->codeAppendf("%s ="
                                   "3.0 * %s.x * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                                   dfdx.c_str(), v.fsIn(), v.fsIn(), dklmdx.c_str(), v.fsIn(),
                                   dklmdx.c_str(), v.fsIn(), dklmdx.c_str());
            fsBuilder->codeAppendf("%s = 3.0 * %s.x * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                                   dfdy.c_str(), v.fsIn(), v.fsIn(), dklmdy.c_str(), v.fsIn(),
                                   dklmdy.c_str(), v.fsIn(), dklmdy.c_str());
            fsBuilder->codeAppendf("%s = vec2(%s, %s);", gF.c_str(), dfdx.c_str(), dfdy.c_str());
            fsBuilder->codeAppendf("%s = sqrt(dot(%s, %s));", gFM.c_str(), gF.c_str(), gF.c_str());
            fsBuilder->codeAppendf("%s = %s.x * %s.x * %s.x - %s.y * %s.z;",
                                   func.c_str(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("%s = %s / %s;",
                                   edgeAlpha.c_str(), func.c_str(), gFM.c_str());
            fsBuilder->codeAppendf("%s = clamp(1.0 - %s, 0.0, 1.0);",
                                   edgeAlpha.c_str(), edgeAlpha.c_str());
            // Add line below for smooth cubic ramp
            // fsBuilder->codeAppendf("%s = %s * %s * (3.0 - 2.0 * %s);",
            //                        edgeAlpha.c_str(), edgeAlpha.c_str(), edgeAlpha.c_str(),
            //                        edgeAlpha.c_str());
            break;
        }
        case kFillBW_GrProcessorEdgeType: {
            fsBuilder->codeAppendf("%s = %s.x * %s.x * %s.x - %s.y * %s.z;",
                                   edgeAlpha.c_str(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn());
            fsBuilder->codeAppendf("%s = float(%s < 0.0);", edgeAlpha.c_str(), edgeAlpha.c_str());
            break;
        }
        default:
            SkFAIL("Shouldn't get here");
    }


    fsBuilder->codeAppendf("%s = vec4(%s);", args.fOutputCoverage, edgeAlpha.c_str());
}

void GrGLCubicEffect::GenKey(const GrGeometryProcessor& processor,
                             const GrBatchTracker&,
                             const GrGLCaps&,
                             GrProcessorKeyBuilder* b) {
    const GrCubicEffect& ce = processor.cast<GrCubicEffect>();
    uint32_t key = ce.isAntiAliased() ? (ce.isFilled() ? 0x0 : 0x1) : 0x2;
    b->add32(key);
}

//////////////////////////////////////////////////////////////////////////////

GrCubicEffect::~GrCubicEffect() {}

void GrCubicEffect::getGLProcessorKey(const GrBatchTracker& bt,
                                      const GrGLCaps& caps,
                                      GrProcessorKeyBuilder* b) const {
    GrGLCubicEffect::GenKey(*this, bt, caps, b);
}

GrGLGeometryProcessor* GrCubicEffect::createGLInstance(const GrBatchTracker& bt) const {
    return SkNEW_ARGS(GrGLCubicEffect, (*this, bt));
}

GrCubicEffect::GrCubicEffect(GrColor color, GrPrimitiveEdgeType edgeType)
    : INHERITED(color), fEdgeType(edgeType) {
    this->initClassID<GrCubicEffect>();
    fInPosition = &this->addVertexAttrib(GrAttribute("inPosition", kVec2f_GrVertexAttribType));
    fInCubicCoeffs = &this->addVertexAttrib(GrAttribute("inCubicCoeffs",
                                                        kVec4f_GrVertexAttribType));
}

bool GrCubicEffect::onIsEqual(const GrGeometryProcessor& other) const {
    const GrCubicEffect& ce = other.cast<GrCubicEffect>();
    return (ce.fEdgeType == fEdgeType);
}

//////////////////////////////////////////////////////////////////////////////

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(GrCubicEffect);

GrGeometryProcessor* GrCubicEffect::TestCreate(SkRandom* random,
                                               GrContext*,
                                               const GrDrawTargetCaps& caps,
                                               GrTexture*[]) {
    GrGeometryProcessor* gp;
    do {
        GrPrimitiveEdgeType edgeType = static_cast<GrPrimitiveEdgeType>(
                                                    random->nextULessThan(kGrProcessorEdgeTypeCnt));
        gp = GrCubicEffect::Create(GrRandomColor(random), edgeType, caps);
    } while (NULL == gp);
    return gp;
}


#pragma once
#include "Part.hpp"
#include <gp_Pln.hxx>
namespace cad::geometry {
struct ModelingProfile {
    TopoDS_Shape shape;
    gp_Pln plane;
    bool open = false;
};
// Selection/thickness maps must first pass the versioned ModelingIntent parser.
ModelingProfile modelingProfile(const sketch::Sketch &sketch, const QCborMap &selection,
                                const QCborMap &thickness);
} // namespace cad::geometry

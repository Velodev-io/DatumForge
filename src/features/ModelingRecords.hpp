#pragma once
#include "ModelingIntent.hpp"
namespace cad::features {
void validateModelingRecords(const QCborMap &records);
domain::GeometryReference intentReference(const QCborMap &intent, QString path);
QCborArray replaceIntentReference(const QCborArray &intents, QString owner, QString path,
                                  const domain::GeometryReference &replacement);
} // namespace cad::features

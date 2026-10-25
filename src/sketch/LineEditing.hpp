#pragma once
#include "Sketch.hpp"
#include <QSet>
namespace cad::sketch {
QStringList lineEditModes();
// Exact worker-only adapter. Candidate is never installed implicitly.
QCborMap previewLineEdit(const Sketch &source, const QCborMap &request);
void validateEditHistory(const Sketch &s, QSet<QString> &identities);
} // namespace cad::sketch

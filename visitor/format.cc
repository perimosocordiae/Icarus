#include "visitor/format.h"

#include "base/log.h"

namespace visitor {
void Format::operator()(ast::Node const *node) const { DEBUG_LOG()(":P"); }
}  // namespace visitor

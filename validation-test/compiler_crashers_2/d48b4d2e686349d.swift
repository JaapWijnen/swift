// {"kind":"typecheck","signature":"swift::constraints::ConstraintSystem::recordArgumentList(swift::constraints::ConstraintLocator*, swift::ArgumentList*)"}
// RUN: not --crash %target-swift-frontend -typecheck %s
protocol b let b = { (a : b)in switch a { case .c(d(

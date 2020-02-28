#include <torch/csrc/jit/api/function_impl.h>
#include <torch/csrc/jit/passes/inliner.h>

#include <torch/csrc/jit/frontend/error_report.h>

namespace torch {
namespace jit {
namespace {
FunctionSchema defaultSchemaFor(const FunctionImpl& function) {
  std::vector<Argument> args;
  std::vector<Argument> returns;
  Graph& g = *function.graph();
  size_t num_inputs = function.num_inputs();
  for (size_t i = 0; i < num_inputs; ++i) {
    const Value* v = g.inputs().at(i);
    std::string name = v->hasDebugName() ? v->debugNameBase()
                                         : ("argument_" + c10::to_string(i));
    args.emplace_back(std::move(name), unshapedType(g.inputs()[i]->type()));
  }
  for (size_t i = 0; i < g.outputs().size(); ++i) {
    returns.emplace_back("", unshapedType(g.outputs()[i]->type()));
  }
  return {function.name(), "", std::move(args), std::move(returns)};
}
} // namespace

void placeholderCreator(FunctionImpl&) {
  throw RecursiveMethodCallError();
}

void FunctionImpl::run(Stack& stack) {
  get_executor().run(stack);
}

void FunctionImpl::run(Stack&& stack) {
  run(stack);
}

IValue FunctionImpl::operator()(
    std::vector<IValue> stack,
    const Kwargs& kwargs) {
  getSchema().checkAndNormalizeInputs(stack, kwargs);
  run(stack);
  return stack.front();
}

void FunctionImpl::ensure_defined() {
  if (function_creator_) {
    auto creator = function_creator_;
    function_creator_ = placeholderCreator;
    creator(*this);
    function_creator_ = nullptr;
  }
  check_single_output();
}

const FunctionSchema& FunctionImpl::getSchema() const {
  if (schema_ == nullptr) {
    schema_ = make_unique<FunctionSchema>(defaultSchemaFor(*this));
  }
  return *schema_;
}

void preoptimizeGraph(std::shared_ptr<Graph>& graph) {
  // TODO: Invoke cleanup passes before and after inlining to reduce amount of
  // code we're copying.
  Inline(*graph);
}

} // namespace jit
} // namespace torch
// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/function-tester.h"

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/codegen/assembler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/execution/execution.h"
#include "src/handles/handles.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

FunctionTester::FunctionTester(const char* source, uint32_t flags)
    : isolate(main_isolate()),
      function((v8_flags.allow_natives_syntax = true, NewFunction(source))),
      flags_(flags) {
  Compile(function);
  const uint32_t supported_flags = OptimizedCompilationInfo::kInlining;
  CHECK_EQ(0u, flags_ & ~supported_flags);
}

FunctionTester::FunctionTester(Graph* graph, int param_count)
    : isolate(main_isolate()),
      function(NewFunction(BuildFunction(param_count).c_str())),
      flags_(0) {
  CompileGraph(graph);
}

FunctionTester::FunctionTester(Handle<Code> code, int param_count)
    : isolate(main_isolate()),
      function((v8_flags.allow_natives_syntax = true,
                NewFunction(BuildFunction(param_count).c_str()))),
      flags_(0) {
  CHECK(!code.is_null());
  CHECK(code->IsCode());
  Compile(function);
  function->set_code(*code, kReleaseStore);
}

FunctionTester::FunctionTester(Handle<Code> code) : FunctionTester(code, 0) {}

void FunctionTester::CheckThrows(Handle<Object> a) {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> no_result = Call(a);
  CHECK(isolate->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate->OptionalRescheduleException(true);
}

void FunctionTester::CheckThrows(Handle<Object> a, Handle<Object> b) {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> no_result = Call(a, b);
  CHECK(isolate->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate->OptionalRescheduleException(true);
}

v8::Local<v8::Message> FunctionTester::CheckThrowsReturnMessage(
    Handle<Object> a, Handle<Object> b) {
  TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  MaybeHandle<Object> no_result = Call(a, b);
  CHECK(isolate->has_pending_exception());
  CHECK(try_catch.HasCaught());
  CHECK(no_result.is_null());
  isolate->OptionalRescheduleException(true);
  CHECK(!try_catch.Message().IsEmpty());
  return try_catch.Message();
}

void FunctionTester::CheckCall(Handle<Object> expected, Handle<Object> a,
                               Handle<Object> b, Handle<Object> c,
                               Handle<Object> d) {
  Handle<Object> result = Call(a, b, c, d).ToHandleChecked();
  CHECK(expected->SameValue(*result));
}

Handle<JSFunction> FunctionTester::NewFunction(const char* source) {
  return Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(source))));
}

Handle<JSObject> FunctionTester::NewObject(const char* source) {
  return Handle<JSObject>::cast(
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(CompileRun(source))));
}

Handle<String> FunctionTester::Val(const char* string) {
  return isolate->factory()->InternalizeUtf8String(string);
}

Handle<Object> FunctionTester::Val(double value) {
  return isolate->factory()->NewNumber(value);
}

Handle<Object> FunctionTester::infinity() {
  return isolate->factory()->infinity_value();
}

Handle<Object> FunctionTester::minus_infinity() { return Val(-V8_INFINITY); }

Handle<Object> FunctionTester::nan() { return isolate->factory()->nan_value(); }

Handle<Object> FunctionTester::undefined() {
  return isolate->factory()->undefined_value();
}

Handle<Object> FunctionTester::null() {
  return isolate->factory()->null_value();
}

Handle<Object> FunctionTester::true_value() {
  return isolate->factory()->true_value();
}

Handle<Object> FunctionTester::false_value() {
  return isolate->factory()->false_value();
}

Handle<JSFunction> FunctionTester::ForMachineGraph(Graph* graph,
                                                   int param_count) {
  JSFunction p;
  {  // because of the implicit handle scope of FunctionTester.
    FunctionTester f(graph, param_count);
    p = *f.function;
  }
  return Handle<JSFunction>(
      p, p->GetIsolate());  // allocated in outer handle scope.
}

Handle<JSFunction> FunctionTester::Compile(Handle<JSFunction> f) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  return Optimize(f, &zone, isolate, flags_);
}

// Compile the given machine graph instead of the source of the function
// and replace the JSFunction's code with the result.
Handle<JSFunction> FunctionTester::CompileGraph(Graph* graph) {
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  OptimizedCompilationInfo info(&zone, isolate, shared, function,
                                CodeKind::TURBOFAN);

  auto call_descriptor = Linkage::ComputeIncoming(&zone, &info);
  Handle<Code> code =
      Pipeline::GenerateCodeForTesting(&info, isolate, call_descriptor, graph,
                                       AssemblerOptions::Default(isolate))
          .ToHandleChecked();
  function->set_code(*code, kReleaseStore);
  return function;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

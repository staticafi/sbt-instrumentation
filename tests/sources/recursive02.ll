; ModuleID = 'recursive02.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nounwind uwtable
define void @foo(i32 %count) #0 {
  %1 = alloca i32, align 4
  %foo_pointer = alloca void (i32)*, align 8
  store i32 %count, i32* %1, align 4
  %2 = load i32, i32* %1, align 4
  %3 = icmp sge i32 %2, 0
  br i1 %3, label %4, label %8

; <label>:4                                       ; preds = %0
  store void (i32)* @foo, void (i32)** %foo_pointer, align 8
  %5 = load void (i32)*, void (i32)** %foo_pointer, align 8
  %6 = load i32, i32* %1, align 4
  %7 = add nsw i32 %6, -1
  store i32 %7, i32* %1, align 4
  call void %5(i32 %7)
  br label %8

; <label>:8                                       ; preds = %4, %0
  ret void
}

; Function Attrs: nounwind uwtable
define void @call_foo() #0 {
  call void @foo(i32 10)
  ret void
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %call_foo_pointer = alloca void (...)*, align 8
  store i32 0, i32* %1, align 4
  store void (...)* bitcast (void ()* @call_foo to void (...)*), void (...)** %call_foo_pointer, align 8
  %2 = load void (...)*, void (...)** %call_foo_pointer, align 8
  call void (...) %2()
  ret i32 0
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.0-2ubuntu4 (tags/RELEASE_380/final)"}

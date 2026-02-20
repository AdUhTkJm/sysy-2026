#let postcond(body) = { body }
#let precond(body) = { body }

== Pass 列表

=== CodeGen

#postcond[
  函数会有 `ArgDimAttr` 记录数组参数的维度。
  
  数组的 alloca 会有 `DimAttr` 记录维度。

  全局数组会有 `ConstIArrAttr` 记录维度和初始值。
]

=== EnsureTerminator

#postcond[
  不再存在 break 和 continue。
 
  所有的基本块都由 yield, condition 或者 return 结尾。
]

=== Mem2Reg

#postcond[
  不再存在不是数组的 alloca。
]

=== RaiseAlloca

=== PropagateArray

=== Lower

=== LowerPostSchedule

部分 use-def chain 破裂，DCE 无法继续工作。

=== LateLegalize

#precond[
  已经进行寄存器分配。
]

#postcond[
  不再存在 alloca。
]

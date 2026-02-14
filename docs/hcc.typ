#let postcond(body) = { body }
#let precond(body) = { body }

== Pass 列表

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

#postcond[
  不再存在局部的 alloca。
]

=== PropagateArray

#postcond[
  不再存在是数组的函数参数。
]

=== Lower

#precond[
  所有的数组操作要么针对 alloca，要么针对全局数组。
]

=== LowerPostSchedule

部分 use-def chain 破裂，DCE 无法继续工作。

=== LateLegalize

#precond[
  已经进行寄存器分配。
]

#postcond[
  不再存在 alloca。
]

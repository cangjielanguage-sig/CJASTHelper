@C
@OverflowThrowing
public unsafe func foo(x: Int64, y: Int64): Int64 {
    x + y
}

public class A <: Object {
    private var x: Int64 = 0

    public init() {
        A(1)
    }

    public init(x: Int64) {
        this.x = x
    }

    public func hello() {
        println(x)
    }
}

func foo1(x: ?A) {
    match (x) {
        case Some(v) => v.hello()
        case None => ()
    }
}

func foo2() {
    let lam1 = {=> 1}
    let lam2 = {x: Int64 => x * 2}
    let lam3 = {x: Int64, y: Int64 => x + y}
    lam1() + lam2(1) + lam3(2, 3)
}

func foo3() {
    let v = 1
    match (v) {
        case b: Bool => b
        case _ =>
            throw Exception("exception")
    }
}

func foo4(p0: Int64, p1: ?Int64, p2: Option<Int64>,
    p3: (Int64), p4: (Int64, Int64),
    p5: () -> Unit, p6: (Int64) -> Int64, p7: (Int64, Int64) -> Int64,
    p8: VArray<Int64, $5>
) {}

unsafe main() {
    let res = foo(2, 3)
    println(res)
    foo1(A())
    foo2()
    foo3()
    return 0
}
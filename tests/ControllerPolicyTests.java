import com.ast.re4decomp.ControllerPolicy;
public final class ControllerPolicyTests {
  static void check(boolean value) {
    if (!value)
      throw new AssertionError();
  }
  public static void main(String[] args) {
    ControllerPolicy p = new ControllerPolicy();
    check(!p.accept(-1, ControllerPolicy.rank(true, false), true));
    check(p.accept(8, ControllerPolicy.rank(false, true), true));
    check(!p.accept(9, ControllerPolicy.rank(false, true), true));
    check(!p.accept(-1, ControllerPolicy.rank(true, false), true));
    check(p.accept(8, ControllerPolicy.rank(false, true), true));
    p.reset();
    check(p.accept(-1, ControllerPolicy.rank(true, false), false));
    check(!p.accept(-2, ControllerPolicy.rank(true, false), false));
    check(p.accept(3, ControllerPolicy.rank(false, false), false));
    check(p.accept(8, ControllerPolicy.rank(false, true), true));
    check(!p.accept(3, ControllerPolicy.rank(false, false), true));
    p.reset();
    check(p.accept(9, ControllerPolicy.rank(false, true), true));
    System.out.println("Controller selection, virtual navigation, priority " +
                       "and reset: 11 checks PASS");
  }
}

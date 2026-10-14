import com.ast.re4decomp.GameMenuNavigation;
public final class TestCaseCursor {
 public static void main(String[] args){
  int[] board={1,1,-1,2,-1,1,1,-1,-1,-1,-1,3,3,-1,-1};
  check(GameMenuNavigation.caseCursor(board,5,3,0,1,0)==2,"leave a large item for adjacent empty cell");
  check(GameMenuNavigation.caseCursor(board,5,3,2,1,0)==3,"empty cursor advances one cell");
  check(GameMenuNavigation.caseCursor(board,5,3,0,0,1)==10,"vertical scan leaves current item, not nearest diagonal item");
  check(GameMenuNavigation.caseCursor(board,5,3,3,0,1)==8,"original column retained");
  check(GameMenuNavigation.caseCursor(board,5,3,0,-1,0)==0,"case edge restores old cursor");
  check(GameMenuNavigation.caseCursor(board,5,3,11,1,0)==13,"skip cells of same item");
  System.out.println("Fix15 case cursor PASS: 6 source-behavior checks");
 }
 static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
}

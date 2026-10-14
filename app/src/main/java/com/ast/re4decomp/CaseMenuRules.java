package com.ast.re4decomp;
/** Sscrn/ss_pzzl.cpp: itemCommandType, setCommandId and PieceCommand::move. */
final class CaseMenuRules {
 static final int[] BASE={0,0x10,0x20,0x30,0x40,0x50,0x90,0x60,0xa0,0xb0};
 private static final int[][] ACTIONS={{0,5,6},{0,1,5,6},{0,1,2,5,6},{2,5,6},{4,5,6},{4,2,5,6},{3,5,6},{5,6},{4,0,5,6},{5}};
 static int[] actions(int type){return ACTIONS[Math.max(0,Math.min(9,type))].clone();}
 private static final int[] HEIGHT={65,86,107,64,66,86,64,45,84,25};
 static int height(int type){return HEIGHT[Math.max(0,Math.min(9,type))];}
}

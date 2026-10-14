package com.ast.re4decomp;

import java.util.ArrayList;

/** Original 512x384 message coordinates, shared by drawing and hit testing. */
final class CardMessageLayout {
  final float x,y,width,height;
  final float[][] choices;
  boolean horizontalChoices(){return choices.length>1&&choices[0][1]==choices[1][1];}
  static int advance(int[] glyph,int[][] bearings) {
    if(glyph[0]==0&&glyph[2]!=99)return (int)(8*21f/32);
    int id=glyph[2]==99?1:glyph[0];
    return id<bearings.length?(int)((bearings[id][1]-bearings[id][0])*21f/32):0;
  }
  CardMessageLayout(int[][] glyphs,int[][] bearings,float sourceX,float sourceY) {
    int lines=0;boolean hasChoices=false;
    for(int[]g:glyphs){lines=Math.max(lines,g[1]);hasChoices|=g[2]==99;}
    int[] widths=new int[lines+1];for(int[]g:glyphs)widths[g[1]]+=advance(g,bearings);
    int maximum=0;for(int w:widths)maximum=Math.max(maximum,w);
    width=maximum;height=lines*20+25;
    // Keep the source left alignment inside the centered confirmation block.
    // Scale the completed picture to the device; do not offset it twice.
    x=hasChoices?(512-width)*.5f:sourceX;y=hasChoices?(384-height)*.5f:sourceY;
    ArrayList<float[]> bounds=new ArrayList<>();float pen=x;int line=0;
    for(int[]g:glyphs) {
      if(line!=g[1]){line=g[1];pen=x;}
      if(g[2]==99)bounds.add(new float[]{pen,y+line*20,pen,y+line*20+25});
      pen+=advance(g,bearings);
      if(!bounds.isEmpty()) {
        float[]b=bounds.get(bounds.size()-1);if(b[1]==y+line*20)b[2]=pen;
      }
    }
    choices=bounds.toArray(new float[0][]);
    // The font cells are taller than the source line advance. Adjacent
    // vertical choices share no touch pixels even where transparent cells overlap.
    for(int i=0;i+1<choices.length;++i)if(choices[i+1][1]>choices[i][1])
      choices[i][3]=Math.min(choices[i][3],choices[i+1][1]);
  }
}

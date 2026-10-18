import math, sys
def bez(p0,p1,p2,p3,t):
    u=1-t
    return tuple(u**3*a+3*u*u*t*b+3*u*t*t*c+t**3*d for a,b,c,d in zip(p0,p1,p2,p3))
def blade(base, c1, c2, tip, w0, barbs=(), power=0.9, n=80, span=6):
    """a tapering blade along a cubic centreline; barbs: (t, side +1/-1, length, back) """
    pts=[bez(base,c1,c2,tip,i/n) for i in range(n+1)]
    L=[];R=[];normals=[]
    for i,p in enumerate(pts):
        q=pts[min(i+1,n)]; r=pts[max(i-1,0)]
        dx,dy=q[0]-r[0],q[1]-r[1]; m=math.hypot(dx,dy) or 1
        nx,ny=-dy/m,dx/m; normals.append((nx,ny,dx/m,dy/m))
        w=w0*(1-i/n)**power/2
        L.append((p[0]+nx*w,p[1]+ny*w)); R.append((p[0]-nx*w,p[1]-ny*w))
    for t,side,length,back in sorted(barbs, reverse=True):
        k=int(t*n); nx,ny,tx,ty=normals[k]
        edge = L if side>0 else R
        bx,by=edge[k]
        tipb=(bx+side*nx*length - tx*back, by+side*ny*length - ty*back)
        edge[k:k+span]=[edge[k], tipb, edge[min(k+span,n)]]
    poly=L+R[::-1]
    return "M"+" L".join(f"{x:.1f} {y:.1f}" for x,y in poly)+" Z"
def mirror(path):
    import re
    out=[]; xy=0
    for t in re.findall(r"[MLZ]|-?\d+(?:\.\d+)?", path):
        if t in "MLZ": out.append(t); xy=0
        else:
            v=float(t)
            if xy%2==0: v=512-v
            out.append(f"{v:.1f}"); xy+=1
    return " ".join(out)

cx, cy, r = 256, 250, 104         # cube: centre and corner distance
top=(cx,cy-r); ul=(cx-r*0.866,cy-r/2); ll=(cx-r*0.866,cy+r/2); bot=(cx,cy+r)
# wings as on the Illuminados emblem: broad, out to the side, the end rising to a point,
# a long barb hanging down under the end
wing = blade((ul[0]+14, ul[1]+10), (140, 150), (82, 168), (46, 120), 52,
             barbs=((0.78, -1, 44, 10), (0.50, -1, 16, 8)), power=0.55)
# the side hooks (the loops of the emblem): angular, out from the lower corner to a point,
# back down and in to a second point
lx, ly = ll
claw = (f"M{lx+2:.1f} {ly-18:.1f} L 84 {ly-18:.1f} L 124 {ly+8:.1f} L 160 {ly+84:.1f} "
        f"L 162 {ly+40:.1f} L {lx+10:.1f} {ly+14:.1f} Z")
# the long blade down from the bottom corner, a short point up from the top corner
stem = blade((cx, bot[1]-14), (cx, bot[1]+40), (cx, bot[1]+90), (cx, 506), 30, power=1.1)
point = blade((cx, top[1]+16), (cx, top[1]-8), (cx, top[1]-26), (cx, top[1]-52), 30, power=1.0)
paths=[wing, mirror(wing), claw, mirror(claw), stem, point]
common=open("common.inc").read()
cube=f'''<g fill="none" stroke="#151012" stroke-width="22" stroke-linejoin="miter" stroke-miterlimit="10">
    <path d="M{top[0]} {top[1]} L {512-ul[0]:.1f} {ul[1]} L {512-ll[0]:.1f} {ll[1]} L {bot[0]} {bot[1]} L {ll[0]:.1f} {ll[1]} L {ul[0]:.1f} {ul[1]} Z"/>
    <path d="M{ul[0]:.1f} {ul[1]} L {cx} {cy} L {512-ul[0]:.1f} {ul[1]} M {cx} {cy} L {cx} {bot[1]}"/></g>'''
svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
{common}
  <g filter="url(#rough)">
  <g fill="#151012">{"".join(f'<path d="{p}"/>' for p in paths)}</g>
  {cube}
  </g>
</svg>'''
open(sys.argv[1],"w").write(svg)

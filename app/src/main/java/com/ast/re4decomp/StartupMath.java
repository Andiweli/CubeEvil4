package com.ast.re4decomp;

/** Frame curves from game/hermite.cpp and the title's original ID path basis. */
final class StartupMath {
  // RotMatrix / idSysMove03: Rz * Ry * Rx, followed by the original ID
  // translation. Keep Z until the complete parent chain has been composed.
  static float[] idTransform(float x,float y,float rx,float ry,float rz) {
    double ax=Math.toRadians(rx),ay=Math.toRadians(ry),az=Math.toRadians(rz);
    float sx=(float)Math.sin(ax),cx=(float)Math.cos(ax),sy=(float)Math.sin(ay),cy=(float)Math.cos(ay),sz=(float)Math.sin(az),cz=(float)Math.cos(az);
    return new float[]{cz*cy,cz*sx*sy-sz*cx,cz*cx*sy+sz*sx,x,
        sz*cy,sz*sx*sy+cz*cx,sz*cx*sy-cz*sx,y,-sy,cy*sx,cy*cx,0};
  }
  static float[] concat(float[] a,float[] b) {
    float[] result=new float[12];
    for(int row=0;row<3;++row)for(int col=0;col<4;++col){float v=col==3?a[row*4+3]:0;for(int k=0;k<3;++k)v+=a[row*4+k]*b[k*4+col];result[row*4+col]=v;}
    return result;
  }
  static float[] screenMatrix(float[] m) {
    // The ID plane is Y-up; Canvas is Y-down.
    return new float[]{m[0],-m[1],m[3],-m[4],m[5],-m[7],0,0,1};
  }
  // TITLE_MENU_MOVE toggles the selected odd highlight. Even base labels stay visible.
  static boolean menuVisible(int mark,int cursor){return mark==0||mark==255||(mark&1)==0||mark==(cursor==0?1:cursor==1?5:3);}
  // IDSystem::unitTrans enqueues each group's children before the group.
  // AddOtDirect inserts at the bucket head, reversing that complete tree.
  static int[] drawOrder(int[][]units) {
    java.util.ArrayList<Integer>order=new java.util.ArrayList<>();
    for(int i=units.length-1;i>=0;--i)if(units[i][1]==255)appendDrawOrder(units,i,order,0);
    int[]result=new int[order.size()];for(int i=0;i<result.length;++i)result[i]=order.get(i);return result;
  }
  private static void appendDrawOrder(int[][]units,int index,java.util.ArrayList<Integer>order,int depth){if(depth>32)throw new IllegalArgumentException("Original title parent cycle");order.add(index);if(units[index][2]==1)for(int i=units.length-1;i>=0;--i)if(units[i][1]==units[index][0])appendDrawOrder(units,i,order,depth+1);}
  static float curve(float[][] keys, float time, float fallback, boolean loop) {
    if (keys.length == 0) return fallback;
    float end = keys[keys.length - 1][0];
    if (loop && end > 0 && time >= end) time %= end;
    else time = Math.min(time, end);
    for (int n = 0; n + 1 < keys.length; ++n) {
      float[] a = keys[n], b = keys[n + 1];
      if (time < a[0] || time > b[0]) continue;
      float dt = b[0] - a[0]; if (dt <= 0) return b[1];
      float s = (time - a[0])/dt, s2 = s*s, s3 = s2*s;
      float h01 = -2*s3 + 3*s2, h11 = s3 - s2, h10 = h11 - s2 + s;
      return (1 - h01)*a[1] + h01*b[1] + dt*(h10*a[2] + h11*b[3]);
    }
    return 0; // Hermite_1CurveCalc returns zero outside its authored key range.
  }
  static float[] basis(int n, int order, float time) {
    int m = Math.min(order, n - 1) + 1;
    float[] q = new float[n + m]; float[][] b = new float[n + m][m];
    for (int j = m; j < n; ++j) q[j] = j - m + m*.5f;
    for (int j = n; j < n + m; ++j) q[j] = n - 1;
    for (int i = 0; i < n; ++i) if (q[i] <= time && time < q[i + 1]) b[i][0] = 1;
    if (q[n + m - 2] <= time && time <= q[n + m - 1] + .00001f) b[n - 1][0] = 1;
    for (int j = 1; j < m; ++j) for (int i = 0; i < n; ++i) {
      if (q[i + 1] != q[i + j + 1]) b[i][j] += (q[i + j + 1] - time)*b[i + 1][j - 1]/(q[i + j + 1] - q[i + 1]);
      if (q[i] != q[i + j]) b[i][j] += (time - q[i])*b[i][j - 1]/(q[i + j] - q[i]);
    }
    float[] result = new float[n]; for (int i = 0; i < n; ++i) result[i] = b[i][m - 1]; return result;
  }
  static float[][] coefficients(float[][] points, int order) {
    int n = points.length; if (n < 2) return points;
    float[][] matrix = new float[n][n + 3];
    for (int i = 0; i < n; ++i) { System.arraycopy(basis(n, order, i), 0, matrix[i], 0, n); System.arraycopy(points[i], 0, matrix[i], n, 3); }
    for (int pivot = 0; pivot < n; ++pivot) {
      int best = pivot; for (int j = pivot + 1; j < n; ++j) if (Math.abs(matrix[j][pivot]) > Math.abs(matrix[best][pivot])) best = j;
      float[] swap = matrix[pivot]; matrix[pivot] = matrix[best]; matrix[best] = swap;
      float divisor = matrix[pivot][pivot]; if (Math.abs(divisor) < 1e-8f) throw new IllegalArgumentException("Original title spline is singular");
      for (int j = pivot; j < n + 3; ++j) matrix[pivot][j] /= divisor;
      for (int row = 0; row < n; ++row) if (row != pivot) { float weight = matrix[row][pivot]; for (int j = pivot; j < n + 3; ++j) matrix[row][j] -= weight*matrix[pivot][j]; }
    }
    float[][] result = new float[n][3]; for (int i = 0; i < n; ++i) System.arraycopy(matrix[i], n, result[i], 0, 3); return result;
  }
  static float[] position(float[][] coefficients, int order, float time) {
    int n = coefficients.length; float[] result = new float[3]; if (n == 0) return result;
    float[] weights = basis(n, order, Math.max(0, Math.min(n - 1, time)));
    for (int i = 0; i < n; ++i) for (int k = 0; k < 3; ++k) result[k] += weights[i]*coefficients[i][k];
    return result;
  }
}

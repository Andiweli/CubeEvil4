package org.re4.port;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/** One Vulkan owner per surface. Loading never blocks Android's UI/input loop. */
public final class GraphicsBootstrapView extends SurfaceView implements SurfaceHolder.Callback, Choreographer.FrameCallback {
    private static final class Session {
        final HandlerThread thread=new HandlerThread("RE4 graphics owner");
        final Handler queue;
        long handle; // Only the graphics thread accesses the JNI owner.
        volatile boolean closing,active;
        volatile int frames;
        boolean pending; // Main-thread frame backpressure.
        Session(){thread.start();queue=new Handler(thread.getLooper());}
    }
    private final Handler main=new Handler(Looper.getMainLooper());
    private Session session;
    private volatile String report="Vulkan surface not attached";
    private String sceneRoot,sceneCatalog,sceneEvent,sceneModel="",evidenceSession="phase5a";
    private int sceneRoom=0x120,sceneFrame,sceneCut=-1,debugView;
    public interface SceneObserver { void accept(String report); }
    private SceneObserver sceneObserver;
    private void configurable(){if(session!=null)throw new IllegalStateException("Configure before attaching surface");}
    public GraphicsBootstrapView(Context context){super(context);getHolder().addCallback(this);System.loadLibrary("re4_port_jni");}
    public void configureDebugView(int view){configurable();if(view<0||view>6)throw new IllegalArgumentException("Debug view 0..6");debugView=view;}
    public void configureEvidenceSession(String value){
        configurable();if(value==null||!value.matches("[A-Za-z0-9_-]{1,80}"))throw new IllegalArgumentException("Evidence session must be a safe file component");evidenceSession=value;
    }
    public void configureScene(String root,String catalog,String event,SceneObserver observer){configureScene(root,catalog,0x120,event,0,observer);}
    public void configureScene(String root,String catalog,int room,String event,int frame,SceneObserver observer){
        configurable();if(room<0||room>0xffff||frame<0)throw new IllegalArgumentException("Original room/frame range");
        sceneRoot=root;sceneCatalog=catalog;sceneRoom=room;sceneEvent=event;sceneFrame=frame;sceneObserver=observer;
    }
    public void configureEventModel(String name){configurable();if(name==null||!name.matches("[A-Za-z0-9_]{0,11}"))throw new IllegalArgumentException("Original event model name");sceneModel=name;}
    public void configureEventCut(int cut){configurable();if(cut< -1||cut>32767)throw new IllegalArgumentException("Original event cut");sceneCut=cut;}
    private void publish(Session owner,String value){
        main.post(()->{if(session!=owner||owner.closing)return;report=value;if(sceneObserver!=null)sceneObserver.accept(value);});
    }
    private void failure(Session owner,RuntimeException error){owner.active=false;Log.e("RE4Graphics",error.getMessage(),error);publish(owner,error.getMessage());}
    @Override public void surfaceCreated(SurfaceHolder holder){
        final Session owner=new Session();session=owner;final Surface surface=holder.getSurface();
        report="Loading original resources / Vulkan on graphics owner";
        owner.queue.post(()->{
            if(owner.closing)return;
            try{
                owner.handle=nativeOpen(surface);
                if(sceneRoot!=null)nativeLoadScene(owner.handle,sceneRoot,sceneCatalog,sceneRoom,sceneEvent,sceneFrame,sceneModel,sceneCut,debugView);
                if(!owner.closing){owner.active=true;main.post(()->{if(session==owner&&!owner.closing)Choreographer.getInstance().postFrameCallback(this);});}
            }catch(RuntimeException error){failure(owner,error);}
        });
    }
    @Override public void surfaceChanged(SurfaceHolder holder,int format,int width,int height){/* Native OUT_OF_DATE rebuild on next owner frame. */}
    @Override public void surfaceDestroyed(SurfaceHolder holder){
        final Session owner=session;session=null;Choreographer.getInstance().removeFrameCallback(this);
        if(owner==null)return;owner.closing=true;owner.active=false;
        // Ordered after any in-flight load/frame. No forced suspension or UI join.
        owner.queue.post(()->{try{if(owner.handle!=0){nativeClose(owner.handle);owner.handle=0;}}finally{owner.thread.quitSafely();}});
    }
    @Override public void doFrame(long timeNanos){
        final Session owner=session;if(owner==null||!owner.active||owner.closing)return;
        if(!owner.pending){owner.pending=true;owner.queue.post(()->{
            try{
                if(!owner.closing){String value=sceneRoot==null?nativeFrame(owner.handle,false):nativeSceneFrame(owner.handle,
                    owner.frames==2?getContext().getFilesDir()+"/"+evidenceSession+"-gpu.rgba":null);++owner.frames;publish(owner,value);}
            }catch(RuntimeException error){failure(owner,error);}
            finally{main.post(()->owner.pending=false);}
        });}
        Choreographer.getInstance().postFrameCallback(this);
    }
    public boolean ready(){Session owner=session;return owner!=null&&owner.active&&!owner.closing&&owner.frames>0;}
    public String report(){return report;}
    public String verifyOriginalRuntime(){
        final Session owner=session;if(!ready())throw new IllegalStateException(report);
        FutureTask<String> probe=new FutureTask<>(()->{
            if(owner.closing)throw new IllegalStateException("Surface retired before original probe");
            String value=nativeOriginalProbe(owner.handle,getContext().getCacheDir()+"/graphics-original-probe");
            nativeFrame(owner.handle,true);nativeFrame(owner.handle,false);Log.i("RE4Graphics",value);return value;
        });
        owner.queue.post(probe);
        try{return probe.get(30,TimeUnit.SECONDS);}catch(Exception error){throw new IllegalStateException("Original graphics owner probe failed",error);}
    }
    private static native long nativeOpen(Surface surface);
    private static native void nativeClose(long handle);
    private static native String nativeFrame(long handle,boolean black);
    private static native String nativeOriginalProbe(long handle,String directory);
    private static native void nativeLoadScene(long handle,String root,String catalog,int room,String event,int frame,String model,int cut,int debugView);
    private static native String nativeSceneFrame(long handle,String readback);
}

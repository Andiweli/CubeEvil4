package com.ast.re4decomp;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;
import java.io.File;

/** A real texture in the window, avoiding an opaque View over a SurfaceView underlay. */
final class StartupMovieView extends TextureView implements TextureView.SurfaceTextureListener {
  interface Listener {void prepared();void completed();void failed(String message);}
  private final File source;
  private final int audioOrdinal;
  private final float volume;
  private final Listener listener;
  private final Handler main=new Handler(Looper.getMainLooper());
  private final HandlerThread decoder=new HandlerThread("RE4-Movie");
  private final Handler control;
  private final StartupMovieState state=new StartupMovieState();
  private MediaPlayer player; // Decoder thread owns every MediaPlayer call.
  private Surface output;
  private SurfaceTexture decoderTexture,viewTexture;
  private boolean seekPending,playing;
  private long playbackStarted;
  private volatile int position;
  private volatile boolean closed;

  StartupMovieView(Context context,File source,int position,int soundMode,float volume,Listener listener){super(context);this.source=source;this.position=position;audioOrdinal=soundMode==0?1:0;this.volume=Math.max(0,Math.min(1,volume));this.listener=listener;
    setOpaque(true);setSurfaceTextureListener(this);decoder.start();control=new Handler(decoder.getLooper());
  }
  int position(){return position;}
  void resumeMovie(){if(!closed)control.post(()->{state.foreground(true);syncPlayback();});}
  void pauseMovie(){if(!closed)control.post(()->{state.foreground(false);syncPlayback();});}
  void closeMovie(){if(closed)return;closed=true;final SurfaceTexture texture=viewTexture;viewTexture=null;control.post(()->{state.close();releasePlayer(false);if(texture!=null)texture.release();decoder.quitSafely();});}
  @Override public void onSurfaceTextureAvailable(SurfaceTexture texture,int width,int height){if(closed){texture.release();return;}viewTexture=texture;setTransform(new Matrix());final Surface surface=new Surface(texture);control.post(()->{
      if(closed){surface.release();return;}releasePlayer(true);decoderTexture=texture;output=surface;final long token=state.attach();currentToken=token;
      try {final MediaPlayer current=new MediaPlayer();player=current;current.setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MOVIE).build());current.setSurface(surface);current.setDataSource(source.getAbsolutePath());current.setLooping(false);
        current.setOnVideoSizeChangedListener((mp,w,h)->{if(state.current(token)&&mp==player&&w>0&&h>0)texture.setDefaultBufferSize(w,h);});
        current.setOnPreparedListener(mp->{if(mp!=player||!state.prepared(token))return;try{int ordinal=0;boolean selected=false;MediaPlayer.TrackInfo[]tracks=mp.getTrackInfo();for(int i=0;i<tracks.length;++i)if(tracks[i].getTrackType()==MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO&&ordinal++==audioOrdinal){mp.selectTrack(i);selected=true;break;}if(!selected)throw new IllegalStateException("Movie audio track is missing");mp.setVolume(this.volume,this.volume);mp.setVideoScalingMode(MediaPlayer.VIDEO_SCALING_MODE_SCALE_TO_FIT);
          seekPending=this.position>0;if(seekPending)mp.seekTo(this.position);else syncPlayback();postCallback(()->listener.prepared());
        }catch(RuntimeException failure){fail("Movie preparation: "+failure.getMessage());}});
        current.setOnSeekCompleteListener(mp->{if(mp==player&&state.current(token)){seekPending=false;syncPlayback();}});
        current.setOnCompletionListener(mp->{if(mp==player&&state.current(token)){playing=false;position=0;state.detach();releasePlayer(false);postCallback(listener::completed);}});
        current.setOnErrorListener((mp,what,extra)->{if(mp==player&&state.current(token))fail("Movie output ("+what+", "+extra+")");return true;});
        current.setOnInfoListener((mp,what,extra)->{if(mp==player&&state.current(token)&&what==MediaPlayer.MEDIA_INFO_VIDEO_RENDERING_START)Log.i("RE4Movie","Decoder is rendering original movie: "+source.getName());return false;});
        current.prepareAsync();
      } catch(Exception failure){fail("Could not start movie: "+failure.getMessage());}
    });}
  @Override public void onSurfaceTextureSizeChanged(SurfaceTexture texture,int width,int height){setTransform(new Matrix());}
  @Override public boolean onSurfaceTextureDestroyed(SurfaceTexture texture){if(viewTexture==texture)viewTexture=null;if(!closed)control.post(()->{if(decoderTexture==texture){state.detach();releasePlayer(true);}texture.release();});return false;}
  @Override public void onSurfaceTextureUpdated(SurfaceTexture texture){if(!closed)control.post(()->{if(decoderTexture==texture&&!state.hasPresented()){ // A rendered texture update proves composition received a frame.
      if(state.presented(currentToken)){Log.i("RE4Movie","Original movie texture visibly updated: "+source.getName());}
    }});}
  private long currentToken;
  private final Runnable poll=new Runnable(){@Override public void run(){if(closed||player==null||!playing)return;try{position=player.getCurrentPosition();if(!state.hasPresented()&&System.nanoTime()-playbackStarted>5_000_000_000L){fail("No visible movie frame received");return;}}catch(RuntimeException failure){fail("Movie position: "+failure.getMessage());return;}control.postDelayed(this,250);}};
  private void syncPlayback(){if(player==null)return;try{if(state.canPlay()&&!seekPending){if(!playing){player.start();playing=true;playbackStarted=System.nanoTime();control.removeCallbacks(poll);control.post(poll);}}
      else if(playing){position=player.getCurrentPosition();player.pause();playing=false;control.removeCallbacks(poll);}
    }catch(RuntimeException failure){fail("Movie playback: "+failure.getMessage());}}
  private void fail(String message){Log.w("RE4Movie",message);state.detach();releasePlayer(false);postCallback(()->listener.failed(message));}
  private void postCallback(Runnable callback){main.post(()->{if(!closed)callback.run();});}
  private void releasePlayer(boolean keepPosition){control.removeCallbacks(poll);final MediaPlayer previous=player;player=null;if(previous!=null){if(keepPosition)try{position=previous.getCurrentPosition();}catch(RuntimeException ignored){}previous.setOnPreparedListener(null);previous.setOnCompletionListener(null);previous.setOnErrorListener(null);previous.setOnInfoListener(null);previous.setOnVideoSizeChangedListener(null);previous.setOnSeekCompleteListener(null);try{previous.setSurface(null);}catch(RuntimeException ignored){}previous.release();}if(output!=null){output.release();output=null;}decoderTexture=null;playing=seekPending=false;}
}

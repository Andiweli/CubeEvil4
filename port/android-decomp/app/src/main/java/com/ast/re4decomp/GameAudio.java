package com.ast.re4decomp;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Process;
import android.util.Log;
import java.util.Arrays;

/** Streams native, disc-decoded PCM independently of the render frame rate. */
public final class GameAudio {
  private static final int RATE = 48000;
  private static final int FRAMES = 512;
  private static GameAudio instance;
  private static volatile float volume=1;
  private static volatile int outputMode=1;
  private final Object wake = new Object();
  private final Thread worker;
  private volatile boolean running = true;
  private volatile boolean active;
  private volatile long startupVoiceRequest, startupVoiceDrainedRequest;

  private GameAudio() {
    worker = new Thread(this::run, "RE4-Audio");
    worker.start();
  }

  public static synchronized void start() {
    if (instance == null) instance = new GameAudio();
  }
  public static void setVolume(float value) { volume=Math.max(0,Math.min(1,value)); }
  public static void setOutputMode(int value) { outputMode=Math.max(0,Math.min(2,value)); }

  /** Fence a queued spoken start cue or load chime before leaving title audio. */
  public static synchronized void awaitStartupVoice() {
    if(instance!=null)++instance.startupVoiceRequest;
  }
  public static synchronized boolean startupVoiceDrained() {
    return instance!=null &&
        instance.startupVoiceDrainedRequest==instance.startupVoiceRequest;
  }

  public static synchronized void setActive(boolean enabled) {
    NativeBridge.audioActive(enabled);
    if (instance == null && enabled) instance = new GameAudio();
    if (instance != null) {
      instance.active = enabled;
      synchronized (instance.wake) { instance.wake.notifyAll(); }
    }
  }

  public static synchronized void stop() {
    NativeBridge.audioActive(false);
    final GameAudio previous = instance;
    instance = null;
    if (previous == null) return;
    previous.active = false;
    previous.running = false;
    synchronized (previous.wake) { previous.wake.notifyAll(); }
    previous.worker.interrupt();
    // Blocking AudioTrack writes are at most a few buffered milliseconds while
    // the track plays. Its owner releases it in finally; UI never releases a
    // track concurrently with a write or waits indefinitely for the worker.
    try { previous.worker.join(250); }
    catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
  }

  private void run() {
    Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
    AudioTrack track = null;
    try {
      final int minimum = AudioTrack.getMinBufferSize(
          RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
      if (minimum <= 0) throw new IllegalStateException("PCM output is unavailable");
      final int bufferBytes = Math.max(minimum, FRAMES * 4 * 4);
      track = new AudioTrack(
          new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
              .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build(),
          new AudioFormat.Builder().setSampleRate(RATE)
              .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
              .setEncoding(AudioFormat.ENCODING_PCM_16BIT).build(),
          bufferBytes, AudioTrack.MODE_STREAM, AudioManager.AUDIO_SESSION_ID_GENERATE);
      if (track.getState() != AudioTrack.STATE_INITIALIZED)
        throw new IllegalStateException("Could not start AudioTrack");
      final short[] pcm = new short[FRAMES * 2];
      final AudioPlaybackFence playback=new AudioPlaybackFence();
      long voiceRequest=0, voiceEndFrame=-1;
      boolean playing = false;
      float appliedVolume=-1;
      while (running) {
        if (!active) {
          if (playing) {
            track.pause();
            // Keep the already decoded tail of the title voice across a
            // lifecycle pause. Flushing it would skip those spoken samples.
            if(startupVoiceDrainedRequest==startupVoiceRequest) {
              track.flush();playback.reset();voiceEndFrame=-1;
            }
            playing = false;
          }
          synchronized (wake) {
            while (running && !active) wake.wait();
          }
          continue;
        }
        final int produced = NativeBridge.renderAudio(pcm);
        final long pendingVoice=startupVoiceRequest;
        if(voiceRequest!=pendingVoice) {
          voiceRequest=pendingVoice;voiceEndFrame=-1;
        }
        final boolean decodedVoiceEnded=NativeBridge.startupVoiceEnded();
        if (produced != FRAMES) Arrays.fill(pcm, (short) 0);
        if(outputMode==0)for(int frame=0;frame<FRAMES;++frame){short mono=(short)(((int)pcm[frame*2]+pcm[frame*2+1])/2);pcm[frame*2]=pcm[frame*2+1]=mono;}
        final float requestedVolume=volume;
        if(requestedVolume!=appliedVolume){track.setVolume(requestedVolume);appliedVolume=requestedVolume;}
        if (!playing) { track.play(); playing = true; }
        int offset = 0;
        while (offset < pcm.length && running &&
            (active || startupVoiceDrainedRequest!=startupVoiceRequest)) {
          // The short[] overload performs blocking writes on every supported
          // API level and returns the actual number of shorts accepted.
          final int written = track.write(pcm, offset, pcm.length - offset);
          if (written <= 0)
            throw new IllegalStateException("AudioTrack write error " + written);
          offset += written;
          playback.wrote(written);
        }
        final long playedFrame=playback.playedFrame(track.getPlaybackHeadPosition());
        if(offset==pcm.length && voiceRequest!=startupVoiceDrainedRequest) {
          // The native mixer can finish before AudioTrack has played the PCM
          // it just accepted. Fence that last written block once, then poll
          // the hardware playback head without delaying the UI thread.
          if(decodedVoiceEnded && voiceEndFrame<0)voiceEndFrame=playback.queuedFrame();
          if(voiceEndFrame>=0 && playedFrame>=voiceEndFrame)
            startupVoiceDrainedRequest=voiceRequest;
        }
      }
    } catch (InterruptedException interrupted) {
      Thread.currentThread().interrupt();
    } catch (RuntimeException failure) {
      Log.w("RE4Audio", "Original audio output ended", failure);
    } finally {
      if (track != null) {
        try { track.pause(); track.flush(); }
        catch (IllegalStateException ignored) { /* Device was removed. */ }
        track.release();
      }
    }
  }
}

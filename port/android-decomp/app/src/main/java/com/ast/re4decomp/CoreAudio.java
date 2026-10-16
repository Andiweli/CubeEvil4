package com.ast.re4decomp;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Process;
import android.util.Log;

/**
 * The sound of the core mode: a thread of its own plays what the game mixes
 * (32 kHz stereo, CoreBridge.audio) through an AudioTrack; the blocking
 * writes pace it to the device's clock (the core matches its rate to that).
 * CoreActivity starts it when the game is in front and stops it otherwise.
 */
final class CoreAudio {
  private static final String TAG = "RE4Audio";
  private static final int RATE = 32000;
  private static final int FRAMES = 256;  // 8 ms per write
  private Thread worker;
  private volatile boolean running;

  synchronized void start() {
    if (worker != null) return;
    running = true;
    worker = new Thread(this::run, "RE4-Core-Audio");
    worker.start();
  }

  synchronized void stop() {
    running = false;
    if (worker == null) return;
    worker.interrupt();
    try {
      worker.join(300);
    } catch (InterruptedException interrupted) {
      Thread.currentThread().interrupt();
    }
    worker = null;
  }

  private void run() {
    Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
    AudioTrack track = null;
    try {
      final int minimum = AudioTrack.getMinBufferSize(
          RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
      if (minimum <= 0) throw new IllegalStateException("no PCM output at 32 kHz");
      final int bufferBytes = Math.max(minimum, FRAMES * 4 * 3);
      track = new AudioTrack(
          new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
              .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build(),
          new AudioFormat.Builder().setSampleRate(RATE)
              .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
              .setEncoding(AudioFormat.ENCODING_PCM_16BIT).build(),
          bufferBytes, AudioTrack.MODE_STREAM, AudioManager.AUDIO_SESSION_ID_GENERATE);
      if (track.getState() != AudioTrack.STATE_INITIALIZED)
        throw new IllegalStateException("AudioTrack did not start");
      Log.i(TAG, "sound: AudioTrack 32000 Hz stereo, buffer " + bufferBytes / 4 + " frames (minimum "
          + minimum / 4 + ")");
      final short[] pcm = new short[FRAMES * 2];
      track.play();
      while (running) {
        CoreBridge.audio(pcm, FRAMES);
        int offset = 0;
        while (offset < pcm.length && running) {
          final int written = track.write(pcm, offset, pcm.length - offset);
          if (written < 0) throw new IllegalStateException("AudioTrack write error " + written);
          offset += written;
        }
      }
    } catch (RuntimeException failure) {
      Log.w(TAG, "sound output ended", failure);
    } finally {
      if (track != null) {
        try {
          track.pause();
          track.flush();
        } catch (IllegalStateException ignored) {
          // the output device went away
        }
        track.release();
      }
    }
  }
}

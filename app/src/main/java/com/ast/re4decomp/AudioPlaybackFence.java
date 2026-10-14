package com.ast.re4decomp;

/** AudioTrack stereo PCM accounting. All access belongs to its audio worker. */
final class AudioPlaybackFence {
  private long writtenSamples, headEpoch, lastHead=-1;

  void reset() { writtenSamples=headEpoch=0;lastHead=-1; }
  void wrote(int samples) {
    if(samples<0)throw new IllegalArgumentException("Negative PCM write");
    writtenSamples+=samples;
  }
  long queuedFrame() { return (writtenSamples+1)/2; }
  long playedFrame(int rawPlaybackHead) {
    long head=rawPlaybackHead&0xffffffffL;
    if(lastHead>=0 && head<lastHead)headEpoch+=1L<<32;
    lastHead=head;
    return headEpoch+head;
  }
}

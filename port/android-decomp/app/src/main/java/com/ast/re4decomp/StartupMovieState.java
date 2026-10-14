package com.ast.re4decomp;

/** Serial decoder ownership: an absent surface or background Activity cannot play. */
final class StartupMovieState {
  private long generation;
  private boolean surface,foreground,prepared,presented,closed;
  long attach(){if(closed)return -1;surface=true;prepared=presented=false;return ++generation;}
  void detach(){++generation;surface=prepared=presented=false;}
  boolean prepared(long token){if(!current(token))return false;prepared=true;return true;}
  boolean presented(long token){if(!current(token)||!prepared)return false;presented=true;return true;}
  void foreground(boolean value){foreground=value;}
  boolean current(long token){return !closed&&surface&&token==generation;}
  boolean canPlay(){return !closed&&surface&&foreground&&prepared;}
  boolean hasPresented(){return presented;}
  void close(){detach();closed=true;foreground=false;}
}

  <defs>
    <radialGradient id="bg" cx="50%" cy="45%" r="72%">
      <stop offset="0" stop-color="#cf3330"/><stop offset="1" stop-color="#97191b"/>
    </radialGradient>
    <filter id="paint" x="0" y="0" width="100%" height="100%">
      <feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="4" result="n"/>
      <feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.10 0" result="grain"/>
      <feComposite in="grain" in2="SourceGraphic" operator="over"/>
    </filter>
    <filter id="rough" x="-5%" y="-5%" width="110%" height="110%">
      <feTurbulence type="fractalNoise" baseFrequency="0.06" numOctaves="2" seed="7" result="t"/>
      <feDisplacementMap in="SourceGraphic" in2="t" scale="4" xChannelSelector="R" yChannelSelector="G"/>
    </filter>
  </defs>
  <rect width="512" height="512" fill="url(#bg)" filter="url(#paint)"/>

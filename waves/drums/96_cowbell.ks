/ Classic 808-style Cowbell
N: ms 900
T: !N

/ Two distinct square wave frequencies
F: hz 540
G: hz 800

/ Phase accumulators
P: +\(N#F)
Q: +\(N#G)

/ Square wave partials (up to 9th harmonic)
A: 1 0 0.333 0 0.2 0 0.142 0 0.111

/ Generate the bandlimited square waves
J: P $ A
K: Q $ A

/ Mix them together
Z: J+K

/ Nasal Bandpass filter for that distinct "donk"
B: 1000 1.0
C: B g Z

/ Two-stage envelope for impact + ring
X: e(T*(0-30.0%N))
Y: e(T*(0-8.0%N))

/ Mix and apply envelopes
M: (X*0.6 + Y*0.4) * C

O: 1
META: \"mode:sample one_shot:1\"
M

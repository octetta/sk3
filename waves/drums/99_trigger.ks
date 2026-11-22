/ 808 Trigger Out — low thump ~100Hz, 650ms
N: ms 650
T: !N
E: e(T*(0-6.9%N))
/ no pitch sweep, fixed ~100Hz
F: hz 100
P: +\(N#F)
S: (s P)
/ brief sub attack
Q: e(T*(0-150%N))
R: r T
C: 0.04 f R
W: w (E*S)+(Q*C*.08)

O: 1
META: \"mode:sample one_shot:1\"
W

/ 808 Clave — 2500Hz dominant, 100ms
N: ms 100
T: !N
E: e(T*(0-6.9%N))
F: hz 2500
G: hz 2750
P: +\(N#F)
Q: +\(N#G)
S: (s P)+(s Q)*.5
/ tiny noise snap
V: e(T*(0-30%N))
R: r T
W: w (E*S)+(V*R*.15)

O: 1
META: \"mode:sample one_shot:1\"
W

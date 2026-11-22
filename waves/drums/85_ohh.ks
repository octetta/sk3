/ 808 Open Hi-Hat — very bright, looser and much longer (750ms)
N: ms 750
T: !N
E: e(T*(0-6.9%N))
/ m verb = 1-bit noise
M: m T
/ r = white noise
R: r T
/ HP filtering
L: 0.5 f R
H: R-L
/ More white noise for a sizzly open sound
W: w E*(M*.4+H*.6)

O: 1
META: \"mode:sample one_shot:1\"
W

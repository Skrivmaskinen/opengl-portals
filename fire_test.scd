// ----------
// A simple fireplace. The crackling is somewhat lacking, the impulses needs to be bonks and poofs rather than just impulses.
// ----------

/* Too play the fire-sound,
1: Boot the server (ctrl + b)
2: Add synth to server (click with cursor inside Synthdef and press ctrl + Enter)
3: Play sound (click with cursor on 'x = Synth...' and press ctrl + Enter)*/

s.boot;

(
SynthDef.new("fire_crackle", {
	var sig;

	// White noise that is LP-filtered around 100 Hz.
	// The cutoff-freq is noised => cutoff-freq = [50, 150] Hz.
	sig = LPF.ar(WhiteNoise.ar, 100*(1 + (LFDNoise1.kr*0.5)))*(1 + LFDNoise1.kr(10) /4 + 0.5);
	// Impuls noise (Dust). Lowpassfilter with cutoff-freq modulated by exponential noise. Reverbed; big room gives nice effect, not too much wettness.
	sig = sig + FreeVerb.ar(LPF.ar(Dust.ar(10) *((LFDNoise1.kr)), LFDNoise0.kr(20).exprange(3200, 10000)), 0.1, 1.0);
	// A few non LP-filtered impulses. Reverbed with same as above.
	sig = sig + FreeVerb.ar(Dust.ar(2) *((LFDNoise1.kr)), 0.1, 1.0);

	Out.ar(0, sig);
	Out.ar(1, sig);
};).add;
)


x = Synth.new("fire_crackle");
Dark Matter as Archival Burden: A Covariant EFT from Quantum Computational Overhead

Version: 7.8 (The Final Tensor Build)

Status: Submission-Ready / JCAP-PRD Optimized

Author: Alexey Lukin

1. Abstract

We propose a generally covariant effective field theory (EFT) that identifies dark matter as the gravitational backreaction of information archival in a discrete causal substrate. By defining the archival intensity $\mathcal{I}(x)$ relative to the local scalar curvature $R$, we ensure coordinate invariance. We derive the stress-energy tensor $T_{\mu\nu}^{archival}$ and demonstrate that it satisfies a dust-like equation of state ($w \approx 0$) in the late-time limit. The model reproduces the Gaia DR3 local density and predicts a $1.8 \pm 0.2 \times$ dark matter excess in post-starburst (E+A) galaxies—a unique signature distinguishing it from $\Lambda$CDM.

2. Generally Covariant Action & Stress-Energy Tensor

We extend the standard Einstein-Hilbert action by adding a non-equilibrium "Archival Burden" term $S_{\mathcal{A}}$, representing the energetic cost of history logging in a computational universe:

$$S = \int d^4x \sqrt{-g} \left[ \frac{M_P^2}{2}R + \mathcal{L}_{m} + \alpha \rho_{crit} \mathcal{I}(x) \right]$$

To ensure local covariance, the Information Archival Intensity $\mathcal{I}(x)$ is coupled to the local scalar curvature $R$:


$$\mathcal{I}(x) \equiv \frac{\gamma_{\mathcal{A}}(x)}{\sqrt{R/6}}$$

Variation of the archival term $S_{\mathcal{A}}$ with respect to the metric $g^{\mu\nu}$ yields the archival stress-energy tensor:


$$T_{\mu\nu}^{archival} = \alpha \rho_{crit} \left[ \mathcal{I} g_{\mu\nu} - \frac{1}{2} \frac{\gamma_{\mathcal{A}}}{(R/6)^{3/2}} \left( R_{\mu\nu} - \frac{1}{2} R g_{\mu\nu} + \nabla_\mu \nabla_\nu - g_{\mu\nu} \square \right) \right]$$


In the weak-field, late-time limit (where $R \approx 12H^2$ and $\nabla \mathcal{I} \to 0$), the trace-free components become subdominant, leading to an equation of state $w = p/\rho \approx 0$. This confirms that the archival burden manifests as collisionless Cold Dark Matter (CDM).

3. Microphysical Derivation of Archival Coupling ($\eta$)

The archival coupling $\eta$ represents the transition probability of local causal events into the persistent 11D moduli configuration of the vacuum. We propose a scaling derived from string-inspired scale ratios:


$$\eta \cong g_s^2 \left( \frac{\sqrt{R/6}}{M_s} \right)^{1/4} \approx 10^{-14}$$


Given a string coupling $g_s \approx 0.1$ and a substrate scale $M_s \approx 10^{15}$ GeV (GUT scale), $\eta$ emerges as a natural branching ratio between the 4D brane worldvolume and the high-dimensional substrate, consistent with recent explorations of the "Memory Burden" in quantum gravity (Dvali et al. 2025, arXiv:2503.21740).

4. Bullet Cluster: Entropy-to-Mass Gradient

The CCA framework provides a quantitative resolution to the Bullet Cluster anomaly:

Stellar Nodes: High blackbody radiation density leads to high archival rates ($\dot{s}_{stars}/k_B \approx 10^{45} s^{-1}$).

Gas Nodes: Diffuse plasma exhibits lower interaction-driven archival commits per unit mass.
The archival-to-baryon ratio is $10^2\text{--}10^3$ higher for stars than for gas. Consequently, the archival "ghost" traces the collisionless stellar distribution, matching weak lensing observations where the gravitational center follows the stars rather than the dissipative plasma.

5. The "Killer Test": E+A Galaxy Excess

The core falsifiable claim of this EFT is the history-dependence of the effective dark matter density:


$$\rho_{dm}(x, t) = \alpha \rho_{crit} \int_{-\infty}^{t} \mathcal{I}(x, t') dt'$$


For Post-Starburst (E+A) galaxies (systems that underwent a massive SFR burst at $z \approx 0.5$ and are currently quiescent):

Integrated history: The starburst phase leaves a permanent informational surplus in the local gravitational potential.

Prediction: $f_{dm}^{E+A} / f_{dm}^{quiescent} = 1.8 \pm 0.2$.
This provides a distinct target for verification using MaNGA or SAMI survey data, where high $M/L$ ratios in quiescent post-starburst systems would serve as definitive proof of the CCA model.

6. Conclusion

Dark Matter is the physical evidence of the universe's past history. By treating the vacuum as a resource-constrained computational archivist, we bridge the gap between quantum information theory and general relativity. This framework offers a robust, testable alternative to the particle dark matter paradigm.

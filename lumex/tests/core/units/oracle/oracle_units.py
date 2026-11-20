#!/usr/bin/env python3
"""Independent oracle of the LumexLib units catalog (todo 205, WP 205-D).

Transcribed by the design owner from the standards, separately from the C++
table that the implementer writes. Python 3 standard library only.

Run:  python -I oracle_units.py > oracle-units.csv
      python -I oracle_units.py --cpp > LumexUnitsOracleData.hpp
      python -I oracle_units.py --check LumexUnitsOracleData.hpp

The second form writes the C++ data of the tests (the rows, the prefixes and
the exact conversions); the third compares a file with it (exit status 1 on a
difference), which the CTest units.oracle_up_to_date does.

Two data rows differ from the design owner's file. The id pascal is
pascal_unit here: Win32 headers define pascal as a macro, which breaks
an enumerator of that name. The other one: square_degree has the
international symbol (sq. °) of table Г.1 of ГОСТ 8.417-2024 (the owner's file
had the Russian form in both columns); (°)² is kept as an international alias.

Every row: exact value of the unit in the coherent SI unit of its dimension
(kilogram for mass), written as the standard writes it, as a Fraction, with a
pi power; the script derives the canonical scale (k, p, q): p/q is the reduced
fraction of the value with whole factors of ten moved into k (PEA-354
decision units-1). Sources (all read on 20.11.2026):

  SIB   BIPM SI Brochure, 9th ed. 2019, V4.01 (June 2026)
  G24   ГОСТ 8.417-2024 (meganorm text); T = таблица, Г = приложение Г
  CGPM  10th CGPM 1954 Resolution 4 (SI Brochure Appendix 1, p. 158)
  NIST  NIST SP 811 (2008), Appendix B.8
  CONV  conventional definition (secondary sources, see design.md)

Columns are documented in design.md, section 9.
"""
import csv
import sys
from fractions import Fraction as F

# Dimension order: L M T I Theta N J.
D0 = (0, 0, 0, 0, 0, 0, 0)


def dim(L=0, M=0, T=0, I=0, Th=0, N=0, J=0):
    return (L, M, T, I, Th, N, J)


LEN, MASS, TIME = dim(L=1), dim(M=1), dim(T=1)
CUR, TEMP, AMT, LUM = dim(I=1), dim(Th=1), dim(N=1), dim(J=1)
AREA, VOL = dim(L=2), dim(L=3)
FREQ = dim(T=-1)
FORCE = dim(L=1, M=1, T=-2)
PRESS = dim(L=-1, M=1, T=-2)
ENERGY = dim(L=2, M=1, T=-2)
POWER = dim(L=2, M=1, T=-3)
CHARGE = dim(T=1, I=1)
VOLT = dim(L=2, M=1, T=-3, I=-1)
CAP = dim(L=-2, M=-1, T=4, I=2)
RES = dim(L=2, M=1, T=-3, I=-2)
COND = dim(L=-2, M=-1, T=3, I=2)
FLUX = dim(L=2, M=1, T=-2, I=-1)
FLUXD = dim(M=1, T=-2, I=-1)
IND = dim(L=2, M=1, T=-2, I=-2)
HFIELD = dim(L=-1, I=1)
ILLUM = dim(L=-2, J=1)
DOSE = dim(L=2, T=-2)
EXPOS = dim(M=-1, T=1, I=1)
CATAL = dim(T=-1, N=1)
VEL = dim(L=1, T=-1)
ACC = dim(L=1, T=-2)
DVISC = dim(L=-1, M=1, T=-1)
KVISC = dim(L=2, T=-1)
LINDEN = dim(L=-1, M=1)
RESIST = dim(L=3, M=1, T=-3, I=-2)
OPTPOW = dim(L=-1)

ALL = "all"
NONE = "none"

# id, domain (ISO/IEC 80000 part), quantity kind, standing, system,
# international canonical, international aliases (prefixable flag per symbol
# as "sym" or "sym!" = not prefixable), russian canonical, russian aliases,
# dimension, value (Fraction or None), pi power, offset (Fraction or None),
# factor kind, unit prefix set, compound flag, source.
#
# A canonical symbol is prefixable when the unit prefix set is not "none",
# unless it ends with "!". Aliases follow the same notation.
R = []


def row(uid, domain, kind, standing, system, intl, intl_al, ru, ru_al, d,
        value, pi=0, offset=None, factor="exact", prefixes=ALL,
        compound=False, source=""):
    R.append(dict(id=uid, domain=domain, kind=kind, standing=standing,
                  system=system, intl=intl, intl_aliases=intl_al, ru=ru,
                  ru_aliases=ru_al, dim=d, value=value, pi=pi,
                  offset=offset, factor=factor, prefixes=prefixes,
                  compound=compound, source=source))


ST, ME, TH, EM, LI, AC, PC, AN, GE = (
    "space_and_time", "mechanics", "thermodynamics", "electromagnetism",
    "light_and_radiation", "acoustics", "physical_chemistry",
    "atomic_and_nuclear_physics", "general")

# --- SI base units: SIB Table 2; G24 T1 -----------------------------------
row("metre", ST, "length", "si_base", "si", "m", "", "м", "", LEN, F(1),
    source="SIB Table 2; G24 T1")
row("gram", ME, "mass", "si_base", "si", "g", "", "г", "", MASS, F(1, 1000),
    source="SIB ch. 3 (kilogram rule); G24 T1, 7.5")
row("second", ST, "time", "si_base", "si", "s", "", "с", "", TIME, F(1),
    source="SIB Table 2; G24 T1")
row("ampere", EM, "electric_current", "si_base", "si", "A", "", "А", "",
    CUR, F(1), source="SIB Table 2; G24 T1")
row("kelvin", TH, "thermodynamic_temperature", "si_base", "si", "K",
    "K", "К", "", TEMP, F(1), source="SIB Table 2; G24 T1")
row("mole", PC, "amount_of_substance", "si_base", "si", "mol", "", "моль",
    "", AMT, F(1), source="SIB Table 2; G24 T1")
row("candela", LI, "luminous_intensity", "si_base", "si", "cd", "", "кд",
    "", LUM, F(1), source="SIB Table 2; G24 T1")

# --- The 22 SI units with special names: SIB Table 4; G24 T3 ---------------
S4 = "SIB Table 4; G24 T3"
row("radian", ST, "plane_angle", "si_special", "si", "rad", "", "рад", "",
    D0, F(1), source=S4)
row("steradian", ST, "solid_angle", "si_special", "si", "sr", "", "ср", "",
    D0, F(1), source=S4)
row("hertz", ST, "frequency", "si_special", "si", "Hz", "", "Гц", "", FREQ,
    F(1), source=S4)
row("newton", ME, "force", "si_special", "si", "N", "", "Н", "", FORCE,
    F(1), source=S4)
row("pascal_unit", ME, "pressure", "si_special", "si", "Pa", "", "Па", "",
    PRESS, F(1), source=S4)
row("joule", ME, "energy", "si_special", "si", "J", "", "Дж", "", ENERGY,
    F(1), source=S4)
row("watt", ME, "power", "si_special", "si", "W", "", "Вт", "", POWER, F(1),
    source=S4)
row("coulomb", EM, "electric_charge", "si_special", "si", "C", "", "Кл", "",
    CHARGE, F(1), source=S4)
row("volt", EM, "voltage", "si_special", "si", "V", "", "В", "", VOLT, F(1),
    source=S4)
row("farad", EM, "capacitance", "si_special", "si", "F", "", "Ф", "", CAP,
    F(1), source=S4)
row("ohm", EM, "resistance", "si_special", "si", "Ω", "Ω|ohm",
    "Ом", "", RES, F(1), source=S4)
row("siemens", EM, "conductance", "si_special", "si", "S", "", "См", "",
    COND, F(1), source=S4)
row("weber", EM, "magnetic_flux", "si_special", "si", "Wb", "", "Вб", "",
    FLUX, F(1), source=S4)
row("tesla", EM, "magnetic_flux_density", "si_special", "si", "T", "", "Тл",
    "", FLUXD, F(1), source=S4)
row("henry", EM, "inductance", "si_special", "si", "H", "", "Гн", "", IND,
    F(1), source=S4)
row("degree_celsius", TH, "celsius_temperature", "si_special", "si",
    "°C", "℃", "°C", "℃", TEMP, F(1),
    offset=F(27315, 100),
    source="SIB Table 4 note (f), 2.3.4 (prefixes); G24 T1 note 1, T3")
row("lumen", LI, "luminous_flux", "si_special", "si", "lm", "", "лм", "",
    LUM, F(1), source=S4)
row("lux", LI, "illuminance", "si_special", "si", "lx", "", "лк", "", ILLUM,
    F(1), source=S4)
row("becquerel", AN, "activity", "si_special", "si", "Bq", "", "Бк", "",
    FREQ, F(1), source=S4)
row("gray", AN, "absorbed_dose", "si_special", "si", "Gy", "", "Гр", "",
    DOSE, F(1), source=S4)
row("sievert", AN, "dose_equivalent", "si_special", "si", "Sv", "", "Зв",
    "", DOSE, F(1), source=S4)
row("katal", PC, "catalytic_activity", "si_special", "si", "kat", "", "кат",
    "", CATAL, F(1), source=S4)

# --- Non-SI units allowed on a par with SI: G24 T5; SIB Table 8 -------------
row("tonne", ME, "mass", "accepted", "non_si", "t", "", "т", "", MASS,
    F(1000), source="SIB Table 8; G24 T5")
row("dalton", AN, "mass", "accepted", "non_si", "Da", "u!", "а.е.м.!", "",
    MASS, F(166053906892, 10**38), factor="measured",
    source="SIB Table 8 note (i), CODATA 2022 1.660 539 068 92(52)e-27 kg;"
           " G24 T5 gives CODATA 2018 1.660 539 066 60(50)e-27 кг,"
           " no prefixes on а.е.м. (T5 note 2)")
row("minute", ST, "time", "accepted", "non_si", "min", "", "мин", "", TIME,
    F(60), prefixes=NONE, source="SIB Table 8; G24 T5 note 2")
row("hour", ST, "time", "accepted", "non_si", "h", "", "ч", "", TIME,
    F(3600), prefixes=NONE, source="SIB Table 8; G24 T5 note 2")
row("day", ST, "time", "accepted", "non_si", "d", "", "сут", "", TIME,
    F(86400), prefixes=NONE, source="SIB Table 8; G24 T5 note 2")
row("degree", ST, "plane_angle", "accepted", "non_si", "°", "",
    "°", "", D0, F(1, 180), pi=1, prefixes=NONE,
    source="SIB Table 8; G24 T5 notes 2, 4")
row("arcminute", ST, "plane_angle", "accepted", "non_si", "′", "'!",
    "′", "'!", D0, F(1, 10800), pi=1, prefixes=NONE,
    source="SIB Table 8; G24 T5 notes 2, 4")
row("arcsecond", ST, "plane_angle", "accepted", "non_si", "″!", "as|\"!",
    "″!", "\"!", D0, F(1, 648000), pi=1, prefixes="milli|micro|pico",
    source="SIB Table 8 note (a) (as, mas, µas, pas); G24 T5 notes 2, 4"
           " (no prefixes on ″)")
row("gon", ST, "plane_angle", "accepted", "non_si", "gon", "", "град", "",
    D0, F(1, 200), pi=1, source="G24 T5 (геодезия)")
row("litre", ST, "volume", "accepted", "non_si", "L", "l", "л", "", VOL,
    F(1, 1000),
    source="SIB Table 8 note (d) (l, L); G24 T5 note 5")
row("astronomical_unit", ST, "length", "accepted", "non_si", "au", "ua",
    "а.е.", "", LEN, F(149597870700), prefixes=NONE,
    source="SIB Table 8 note (j) (au, IAU 2012 B2); G24 T5 (ua, а.е.),"
           " note 2")
row("light_year", ST, "length", "accepted", "non_si", "ly", "", "св. год",
    "", LEN, F(9460730472580800), prefixes=NONE, source="G24 T5")
row("parsec", ST, "length", "accepted", "non_si", "pc", "", "пк", "", LEN,
    F(648000 * 149597870700), pi=-1,
    source="IAU 2015 B2: 648000/π au exactly; G24 T5 rounds it to 3.085677e16 m")
row("diopter", ST, "optical_power", "accepted", "non_si", "", "", "дптр",
    "", OPTPOW, F(1), prefixes=NONE, source="G24 T5 note 2")
row("hectare", ST, "area", "accepted", "non_si", "ha", "", "га", "", AREA,
    F(10**4), prefixes=NONE, source="SIB Table 8 note (b); G24 T5")
row("electronvolt", AN, "energy", "accepted", "non_si", "eV", "", "эВ", "",
    ENERGY, F(1602176634, 10**28),
    source="SIB Table 8 note (l) (exact since 2018); G24 T5")
row("kilowatt_hour", ME, "energy", "accepted", "non_si", "kW·h!",
    "kW⋅h!", "кВт·ч!", "кВт⋅ч!", ENERGY, F(3600000),
    prefixes=NONE, compound=True, source="G24 T5")
row("volt_ampere", EM, "apparent_power", "accepted", "non_si", "V·A",
    "V⋅A", "В·А", "В⋅А", POWER, F(1), compound=True,
    source="G24 T5")
row("var", EM, "reactive_power", "accepted", "non_si", "var", "", "вар", "",
    POWER, F(1), source="SIB Table 8 note (n); G24 T5")
row("ampere_hour", EM, "electric_charge", "accepted", "non_si", "A·h",
    "A⋅h", "А·ч", "А⋅ч", CHARGE, F(3600), compound=True,
    source="G24 T5")

# --- Relative and logarithmic units: G24 T6; SIB 5.4.7, Table 8 ------------
row("one", GE, "dimensionless_ratio", "relative_log", "si", "1", "", "1", "",
    D0, F(1), prefixes=NONE, source="SIB 5.4.7; G24 T6")
row("percent", GE, "dimensionless_ratio", "relative_log", "non_si", "%", "",
    "%", "", D0, F(1, 100), prefixes=NONE, source="SIB 5.4.7; G24 T6")
row("permille", GE, "dimensionless_ratio", "relative_log", "non_si",
    "‰", "", "‰", "", D0, F(1, 1000), prefixes=NONE,
    source="G24 T6")
row("ppm", GE, "dimensionless_ratio", "relative_log", "non_si", "ppm", "",
    "млн⁻¹", "млн-1|млн^-1", D0, F(1, 10**6), prefixes=NONE,
    source="SIB 5.4.7; G24 T6")
row("bel", GE, "logarithmic_level", "relative_log", "non_si", "B", "", "Б",
    "", D0, None, factor="none", prefixes=NONE,
    source="SIB Table 8 note (m); G24 T6")
row("decibel", GE, "logarithmic_level", "relative_log", "non_si", "dB", "",
    "дБ", "", D0, None, factor="none", prefixes=NONE,
    source="SIB Table 8 note (m); G24 T6")
row("neper", GE, "logarithmic_level", "relative_log", "non_si", "Np", "",
    "Нп", "", D0, None, factor="none", prefixes=NONE,
    source="SIB Table 8 note (m); G24 T6")
row("phon", AC, "loudness_level", "relative_log", "non_si", "phon", "",
    "фон", "", D0, None, factor="none", prefixes=NONE, source="G24 T6")
row("octave", AC, "frequency_interval", "relative_log", "non_si", "", "",
    "окт", "", D0, None, factor="none", prefixes=NONE, source="G24 T6")
row("decade", AC, "frequency_interval", "relative_log", "non_si", "", "",
    "дек", "", D0, None, factor="none", prefixes=NONE, source="G24 T6")

# --- Temporarily allowed: G24 T7 --------------------------------------------
row("nautical_mile", ST, "length", "temporary", "non_si", "n mile", "",
    "миля", "", LEN, F(1852), prefixes=NONE, source="G24 T7")
row("foot", ST, "length", "temporary", "non_si", "ft", "", "фт", "", LEN,
    F(3048, 10**4), prefixes=NONE, source="G24 T7")
row("angstrom", ST, "length", "temporary", "non_si", "Å",
    "Å", "Å", "Å", LEN, F(1, 10**10), prefixes=NONE,
    source="SIB Table 8 note (f); G24 T7")
row("carat", ME, "mass", "temporary", "non_si", "", "", "кар", "", MASS,
    F(2, 10**4), prefixes=NONE, source="G24 T7")
row("tex", ME, "linear_density", "temporary", "non_si", "tex", "", "текс",
    "", LINDEN, F(1, 10**6), source="G24 T7")
row("knot", ST, "velocity", "temporary", "non_si", "kn", "", "уз", "", VEL,
    F(1852, 3600), prefixes=NONE,
    source="G24 T7 (0.514(4) м/с = 1852 м / 3600 с)")
row("gal", ME, "acceleration", "temporary", "cgs", "Gal", "", "Гал", "",
    ACC, F(1, 100), source="SIB Table 8 note (g); G24 T7")
row("revolution_per_second", ST, "rotational_frequency", "temporary",
    "non_si", "r/s!", "", "об/с!", "", FREQ, F(1), prefixes=NONE,
    compound=True, source="G24 T7 (1 с^-1)")
row("revolution_per_minute", ST, "rotational_frequency", "temporary",
    "non_si", "r/min!", "", "об/мин!", "", FREQ, F(1, 60), prefixes=NONE,
    compound=True, source="G24 T7 (1/60 с^-1)")
row("bar", ME, "pressure", "temporary", "non_si", "bar", "", "бар", "",
    PRESS, F(10**5), source="SIB Table 8 note (h); G24 T7")
row("millimetre_of_mercury", ME, "pressure", "temporary", "non_si", "mm Hg",
    "mmHg", "мм рт. ст.",
    "мм. рт. ст", PRESS,
    F(135951, 10) * F(980665, 10**5) / 1000, prefixes=NONE,
    source="CONV 13595.1 kg/m^3 × 9.80665 m/s^2 × 1 mm; G24 T7 133.3224 Па;"
           " NIST 1.333 224 E+02 (user decision 5, 20.11.2026)")

# --- Annex Г of G24 (and CGS) -----------------------------------------------
row("x_unit", ST, "length", "annex", "non_si", "X", "", "икс-ед.", "", LEN,
    F(100206, 10**18), factor="measured", prefixes=NONE,
    source="G24 Г: 1.00206e-13 м (приблизительно)")
row("barn", AN, "area", "annex", "non_si", "b", "", "б", "", AREA,
    F(1, 10**28), source="SIB Table 8 note (c); G24 Г")
row("centner", ME, "mass", "annex", "non_si", "q", "", "ц", "", MASS,
    F(100), prefixes=NONE, source="G24 Г")
row("square_degree", ST, "solid_angle", "annex", "non_si", "(sq. °)!",
    "(°)²!", "(°)²!", "", D0, F(1, 32400), pi=2, prefixes=NONE,
    source="G24 Г: 3.0462...e-4 ср = (π/180)^2; the international column of"
           " table Г.1 is (sq. °), the Russian one (°)^2")
row("dyne", ME, "force", "annex", "cgs", "dyn", "", "дин", "", FORCE,
    F(1, 10**5), source="G24 Г; NIST")
row("kilogram_force", ME, "force", "annex", "non_si", "kgf", "", "кгс", "",
    FORCE, F(980665, 10**5), prefixes=NONE, source="G24 Г (точно); NIST")
row("kilopond", ME, "force", "annex", "non_si", "kp", "", "", "", FORCE,
    F(980665, 10**5), prefixes=NONE, source="G24 Г (точно)")
row("gram_force", ME, "force", "annex", "non_si", "gf", "", "гс", "", FORCE,
    F(980665, 10**8), prefixes=NONE, source="G24 Г (точно)")
row("pond", ME, "force", "annex", "non_si", "p", "", "", "", FORCE,
    F(980665, 10**8), prefixes=NONE, source="G24 Г (точно)")
row("tonne_force", ME, "force", "annex", "non_si", "tf", "", "тс", "", FORCE,
    F(980665, 100), prefixes=NONE, source="G24 Г (точно)")
row("kilogram_force_per_square_centimetre", ME, "pressure", "annex",
    "non_si", "kgf/cm²!", "kgf/cm2!", "кгс/см²!", "кгс/см2!", PRESS,
    F(980665, 10), prefixes=NONE, compound=True, source="G24 Г (точно)")
row("kilopond_per_square_centimetre", ME, "pressure", "annex", "non_si",
    "kp/cm²!", "kp/cm2!", "", "", PRESS, F(980665, 10), prefixes=NONE,
    compound=True, source="G24 Г (точно)")
row("millimetre_of_water", ME, "pressure", "annex", "non_si",
    "mm H₂O!", "mmH2O!|mm H2O!", "мм вод. ст.!", "", PRESS,
    F(980665, 10**5), prefixes=NONE, source="G24 Г (точно)")
row("torr", ME, "pressure", "annex", "non_si", "Torr", "", "",
    "торр", PRESS,
    F(101325, 760),
    source="1/760 of the standard atmosphere (CGPM); G24 Г 133.322 Па;"
           " NIST 1.333 224 E+02 (user decision 5)")
row("kilopond_per_square_millimetre", ME, "pressure", "annex", "non_si",
    "kp/mm²!", "kp/mm2!", "", "", PRESS, F(980665, 10**5) * 10**6,
    prefixes=NONE, compound=True, source="G24 Г (точно)")
row("erg", ME, "energy", "annex", "cgs", "erg", "", "эрг", "", ENERGY,
    F(1, 10**7), source="G24 Г; NIST")
row("metric_horsepower", ME, "power", "annex", "non_si", "", "", "л.с.", "",
    POWER, F(73549875, 10**5), prefixes=NONE,
    source="75 kgf*m/s = 735.49875 W exactly; G24 Г prints 735.499 Вт")
row("poise", ME, "dynamic_viscosity", "annex", "cgs", "P", "", "П", "",
    DVISC, F(1, 10), source="G24 Г; NIST")
row("stokes", ME, "kinematic_viscosity", "annex", "cgs", "St", "", "Ст", "",
    KVISC, F(1, 10**4), source="G24 Г; NIST")
row("ohm_square_millimetre_per_metre", EM, "resistivity", "annex", "non_si",
    "Ω·mm²/m!", "Ω·mm2/m!", "Ом·мм²/м!",
    "Ом·мм2/м!", RESIST, F(1, 10**6), prefixes=NONE, compound=True,
    source="G24 Г")
row("maxwell", EM, "magnetic_flux", "annex", "cgs", "Mx", "", "Мкс", "",
    FLUX, F(1, 10**8), source="G24 Г; NIST (correspondence)")
row("gauss", EM, "magnetic_flux_density", "annex", "cgs", "Gs", "G", "Гс",
    "", FLUXD, F(1, 10**4),
    source="G24 Г (Gs, Гс), NIST (Gs, G); Gs canonical, G accepted")
row("gilbert", EM, "magnetomotive_force", "annex", "cgs", "Gb", "Gi!", "Гб",
    "", CUR, F(10, 4), pi=-1,
    source="G24 Г (10/4π А); NIST (Gi) 7.957 747 E-01")
row("oersted", EM, "magnetic_field_strength", "annex", "cgs", "Oe", "", "Э",
    "", HFIELD, F(1000, 4), pi=-1,
    source="G24 Г (10^3/4π А/м); NIST 7.957 747 E+01")
row("calorie", TH, "energy", "annex", "non_si", "cal", "", "кал", "", ENERGY,
    F(41868, 10**4), source="G24 Г калория (международная), точно; NIST cal_IT")
row("rad_dose", AN, "absorbed_dose", "annex", "non_si", "rd", "", "", "",
    DOSE, F(1, 100),
    source="G24 Г (rad, rd / рад); rad and рад belong to the radian"
           " (user decision 7)")
row("rem", AN, "dose_equivalent", "annex", "non_si", "rem", "", "бэр", "",
    DOSE, F(1, 100), source="G24 Г; NIST")
row("roentgen", AN, "exposure", "annex", "non_si", "R", "", "Р", "", EXPOS,
    F(258, 10**6), source="G24 Г (точно); NIST")
row("curie", AN, "activity", "annex", "non_si", "Ci", "", "Ки", "", FREQ,
    F(37 * 10**9), source="G24 Г (точно); NIST")
row("micron", ST, "length", "annex", "non_si", "µ", "μ", "мк", "",
    LEN, F(1, 10**6), prefixes=NONE, source="G24 Г")
row("revolution", ST, "plane_angle", "annex", "non_si", "r", "", "об", "",
    D0, F(2), pi=1, prefixes=NONE, source="G24 Г (2π рад)")
row("ampere_turn", EM, "magnetomotive_force", "annex", "non_si", "At", "",
    "ав", "", CUR, F(1), prefixes=NONE, source="G24 Г")
row("nit", LI, "luminance", "annex", "non_si", "nt", "", "нт", "", ILLUM,
    F(1), prefixes=NONE, source="G24 Г")
row("are", ST, "area", "annex", "non_si", "a", "", "а", "", AREA, F(100),
    prefixes=NONE, source="SIB Table 8 note (b); G24 Г")

# --- Outside ГОСТ: the standard atmosphere ----------------------------------
row("standard_atmosphere", ME, "pressure", "cgpm", "non_si", "atm", "", "",
    "атм", PRESS, F(101325), prefixes=NONE,
    source="CGPM 1954 Res. 4 (SIB Appendix 1 p. 158); NIST exact;"
           " not in G24; ru alias is input only")


def canon(value):
    n, d, k = value.numerator, value.denominator, 0
    while n % 10 == 0:
        n //= 10
        k += 1
    while d % 10 == 0:
        d //= 10
        k -= 1
    return k, n, d


def main():
    out = csv.writer(sys.stdout, delimiter=";", lineterminator="\n")
    out.writerow(["id", "domain", "kind", "standing", "system", "intl",
                  "intl_aliases", "ru", "ru_aliases", "L", "M", "T", "I",
                  "Th", "N", "J", "k", "p", "q", "pi", "offset_k",
                  "offset_p", "offset_q", "factor", "prefixes", "compound",
                  "source"])
    ids = set()
    for r in R:
        assert r["id"] not in ids, r["id"]
        ids.add(r["id"])
        if r["value"] is None:
            k = p = q = ""
        else:
            k, p, q = canon(r["value"])
            assert abs(p) < 2**63 and q < 2**63, r["id"]
        if r["offset"] is None:
            ok = op = oq = ""
        else:
            ok, op, oq = canon(r["offset"])
        out.writerow([r["id"], r["domain"], r["kind"], r["standing"],
                      r["system"], r["intl"], r["intl_aliases"], r["ru"],
                      r["ru_aliases"], *r["dim"], k, p, q, r["pi"], ok, op,
                      oq, r["factor"], r["prefixes"], int(r["compound"]),
                      r["source"]])


# --- The C++ data of the tests (--cpp) --------------------------------------
Fraction = F
# The prefixes: SIB Table 7 and ГОСТ 8.417-2024 table 8; the micro sign is
# U+00B5 in both outputs, U+03BC and u are accepted on input.
PREFIXES = [
    ("quecto", -30, "q", "кв", ""), ("ronto", -27, "r", "рн", ""),
    ("yocto", -24, "y", "и", ""), ("zepto", -21, "z", "з", ""),
    ("atto", -18, "a", "а", ""), ("femto", -15, "f", "ф", ""),
    ("pico", -12, "p", "п", ""), ("nano", -9, "n", "н", ""),
    ("micro", -6, "µ", "мк", "μ|u"),
    ("milli", -3, "m", "м", ""), ("centi", -2, "c", "с", ""),
    ("deci", -1, "d", "д", ""), ("deca", 1, "da", "да", ""),
    ("hecto", 2, "h", "г", ""), ("kilo", 3, "k", "к", ""),
    ("mega", 6, "M", "М", ""), ("giga", 9, "G", "Г", ""),
    ("tera", 12, "T", "Т", ""), ("peta", 15, "P", "П", ""),
    ("exa", 18, "E", "Э", ""), ("zetta", 21, "Z", "З", ""),
    ("yotta", 24, "Y", "И", ""), ("ronna", 27, "R", "Рн", ""),
    ("quetta", 30, "Q", "Кв", ""),
]

PI_DIGITS = "3.14159265358979323846264338327950288419716939937510582097494459"


def cpp_literal(s):
    """UTF-8 bytes as \\x escapes in adjacent literals: an escape swallows the
    hexadecimal digits that follow it, so the literal is closed after every
    non-ASCII character."""
    pieces = []
    cur = ""
    for idx, ch in enumerate(s):
        if ord(ch) < 128:
            cur += ("\\" + ch) if ch in '"\\' else ch
        else:
            cur += "".join("\\x%02X" % b for b in ch.encode("utf-8"))
            if idx + 1 < len(s):
                pieces.append(cur)
                cur = ""
    pieces.append(cur)
    return " ".join('"%s"' % p for p in pieces)


def _expected(x, value_from, pi_from, value_to, pi_to):
    """The correctly rounded double of x * value_from / value_to *
    pi^(pi_from - pi_to), computed exactly."""
    from decimal import Decimal, getcontext
    exact = Fraction(x) * value_from / value_to
    n = pi_from - pi_to
    if n == 0:
        return float(exact), False
    getcontext().prec = 90
    pi = Decimal(PI_DIGITS)
    d = Decimal(exact.numerator) / Decimal(exact.denominator)
    d = d * (pi ** n)
    return float(d), True


def check_normalization():
    """Every canonical symbol is in Unicode normalization form C: a text that
    a consumer normalizes with NFC keeps the symbol the catalog writes."""
    import unicodedata
    for r in R:
        for key in ("intl", "ru"):
            sym = r[key].rstrip("!")
            assert unicodedata.normalize("NFC", sym) == sym, (r["id"], key)


# --- The sweep of the conversions (todo 205, round 4) ----------------------
# The correctly rounded double of x * r for pairs of (unit, prefix) of one
# quantity kind, r the exact ratio of the two values, computed with integers
# (a Fraction) and, with pi, with 200 digits. The pairs are
# chosen without a random generator of the library (a fixed linear
# congruential one), so the file is the same on every Python 3.
PI_LONG = "3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196"
SWEEP_INPUTS = (1.0, 700.0, 0.1, 1.0 / 3.0, 123456.789, 1e150, 1e-150,
                0.9999999999999999)


class _Lcg(object):
    def __init__(self, seed):
        self.state = seed

    def next(self, bound):
        self.state = (self.state * 6364136223846793005
                      + 1442695040888963407) % (1 << 64)
        return (self.state >> 33) % bound


def _sweep_exact(x, ratio, pi_power):
    """The correctly rounded double of x * ratio * pi ** pi_power."""
    import struct
    from decimal import Decimal, getcontext
    exact = F(x) * ratio
    if pi_power == 0:
        value = float(exact)
    else:
        getcontext().prec = 200
        pi = Decimal(PI_LONG)
        value = float(Decimal(exact.numerator) / Decimal(exact.denominator)
                      * (pi ** pi_power))
    assert 2.3e-308 < abs(value) < 1.7e308, (x, ratio, pi_power)
    return struct.unpack("<Q", struct.pack("<d", value))[0]


def sweep_data():
    """The items (unit, prefix) and the pairs of the sweep: (from, to, the
    four inputs as a selection, the four expected bit patterns)."""
    exponents = dict((name, exp) for name, exp, _, _, _ in PREFIXES)
    kinds = {}
    for r in R:
        if r["factor"] != "none" and r["offset"] is None:
            kinds.setdefault(r["kind"], []).append(r)

    def prefixes_of(r):
        if r["prefixes"] == "all":
            return [name for name, _, _, _, _ in PREFIXES]
        if r["prefixes"] == "none":
            return []
        return [n for n, _, _, _, _ in PREFIXES
                if n in r["prefixes"].split("|")]

    items = []
    index = {}

    def item(r, prefix):
        key = (r["id"], prefix)
        if key not in index:
            index[key] = len(items)
            items.append((r, prefix))
        return index[key]

    pairs = []
    seen = set()

    def add(a, pa, b, pb):
        if a is b and pa == pb:
            return
        key = (a["id"], pa, b["id"], pb)
        if key not in seen:
            seen.add(key)
            pairs.append((item(a, pa), item(b, pb)))

    rng = _Lcg(205)
    for kind in sorted(kinds):
        units = kinds[kind]
        anchor = units[0]
        for a in units:
            for b in units:
                if a is b:
                    continue
                add(a, "none", b, "none")
                # two prefixes of the allowed sets, chosen by the generator,
                # and the extremes of both
                pa_all = ["none"] + prefixes_of(a)
                pb_all = ["none"] + prefixes_of(b)
                for _ in range(10):
                    add(a, pa_all[rng.next(len(pa_all))],
                        b, pb_all[rng.next(len(pb_all))])
                for pa in ("quecto", "quetta"):
                    for pb in ("quecto", "quetta"):
                        if pa in pa_all and pb in pb_all and \
                                (a["pi"] != b["pi"] or rng.next(4) == 0):
                            add(a, pa, b, pb)
            for prefix in prefixes_of(a):
                add(a, prefix, anchor, "none")
                add(anchor, "none", a, prefix)
            # the same unit in two prefixes
            all_a = ["none"] + prefixes_of(a)
            for _ in range(30):
                add(a, all_a[rng.next(len(all_a))],
                    a, all_a[rng.next(len(all_a))])
    rows = []
    for number, (fi, ti) in enumerate(pairs):
        (a, pa), (b, pb) = items[fi], items[ti]
        ratio = (a["value"] * F(10) ** exponents.get(pa, 0)
                 / (b["value"] * F(10) ** exponents.get(pb, 0)))
        pi_power = a["pi"] - b["pi"]
        chosen = [0]
        while len(chosen) < 4:
            candidate = 1 + rng.next(len(SWEEP_INPUTS) - 1)
            if candidate not in chosen:
                chosen.append(candidate)
        selection = sum(c << (4 * i) for i, c in enumerate(chosen))
        expected = [_sweep_exact(SWEEP_INPUTS[c], ratio, pi_power)
                    for c in chosen]
        rows.append((fi, ti, selection, expected))
    return [(r["id"], p) for r, p in items], rows


def cpp_text():
    check_normalization()
    def scale_value(r, prefix_exp):
        return r["value"] * Fraction(10) ** prefix_exp

    out = []
    w = out.append
    w("// clang-format off")
    w("// Generated by oracle_units.py --cpp. Do not edit: the CTest")
    w("// units.oracle_up_to_date regenerates this file and fails on a")
    w("// difference.")
    w("#ifndef LUMEX_TESTS_CORE_UNITS_ORACLE_DATA_HPP")
    w("#define LUMEX_TESTS_CORE_UNITS_ORACLE_DATA_HPP")
    w("")
    w("#include <cstddef>")
    w("")
    w("namespace lumex_test_units")
    w("{")
    w("// One row per unit; the symbols are UTF-8, a trailing ! marks a symbol")
    w("// that takes no prefix, the aliases are separated by |.")
    w("struct oracle_row_t")
    w("{")
    for line in ("char const *id;", "char const *domain;", "char const *kind;",
                 "char const *standing;", "char const *system;",
                 "char const *intl;", "char const *intl_aliases;",
                 "char const *ru;", "char const *ru_aliases;",
                 "int dimension[7];", "bool has_value;", "int k;",
                 "long long p;", "long long q;", "int pi;",
                 "bool has_offset;", "int offset_k;", "long long offset_p;",
                 "long long offset_q;", "char const *factor;",
                 "char const *prefixes;", "bool compound;"):
        w("  " + line)
    w("};")
    w("")
    w("struct oracle_prefix_t")
    w("{")
    w("  char const *name;")
    w("  int exponent;")
    w("  char const *intl;")
    w("  char const *ru;")
    w("  char const *intl_aliases;")
    w("};")
    w("")
    w("// A conversion of `input` between two units (prefix name or none): the")
    w("// correctly rounded double of the exact result; has_pi is set when the")
    w("// factor contains pi, which is not rational.")
    w("struct oracle_conversion_t")
    w("{")
    w("  char const *from;")
    w("  char const *from_prefix;")
    w("  char const *to;")
    w("  char const *to_prefix;")
    w("  double input;")
    w("  double expected;")
    w("  bool has_pi;")
    w("};")
    w("")
    w("static oracle_row_t const oracle_rows[] = {")
    for r in R:
        if r["value"] is None:
            has_value, (k, p, q) = False, (0, 0, 1)
        else:
            has_value, (k, p, q) = True, canon(r["value"])
        if r["offset"] is None:
            has_off, (ok, op, oq) = False, (0, 0, 1)
        else:
            has_off, (ok, op, oq) = True, canon(r["offset"])
        d = ", ".join(str(x) for x in r["dim"])
        w("  { %s, %s, %s, %s, %s, %s, %s, %s, %s, { %s }, %s, %d, %d, %d, %d,"
          " %s, %d, %d, %d, %s, %s, %s }," % (
              cpp_literal(r["id"]), cpp_literal(r["domain"]),
              cpp_literal(r["kind"]), cpp_literal(r["standing"]),
              cpp_literal(r["system"]), cpp_literal(r["intl"]),
              cpp_literal(r["intl_aliases"]), cpp_literal(r["ru"]),
              cpp_literal(r["ru_aliases"]), d,
              "true" if has_value else "false", k, p, q, r["pi"],
              "true" if has_off else "false", ok, op, oq,
              cpp_literal(r["factor"]), cpp_literal(r["prefixes"]),
              "true" if r["compound"] else "false"))
    w("};")
    w("")
    w("static oracle_prefix_t const oracle_prefixes[] = {")
    for name, exp, i, ru, al in PREFIXES:
        w("  { %s, %d, %s, %s, %s }," % (cpp_literal(name), exp,
                                         cpp_literal(i), cpp_literal(ru),
                                         cpp_literal(al)))
    w("};")
    w("")

    # Conversions of units of one kind (no offset, a value), prefix none; then
    # prefixed samples.
    convs = []
    byid = {r["id"]: r for r in R}
    kinds = {}
    for r in R:
        if r["factor"] != "none" and r["offset"] is None:
            kinds.setdefault(r["kind"], []).append(r)
    inputs = (1.0, 700.0)
    for kind, units in kinds.items():
        for a in units:
            for b in units:
                if a is b:
                    continue
                for x in inputs:
                    e, has_pi = _expected(x, a["value"], a["pi"], b["value"],
                                          b["pi"])
                    convs.append((a["id"], "none", b["id"], "none", x, e,
                                  has_pi))
    prefix_exp = {name: exp for name, exp, _, _, _ in PREFIXES}
    names = [name for name, _, _, _, _ in PREFIXES] + ["none"]
    some = ["none", "kilo", "milli", "micro", "nano", "mega"]
    for uid, ins, chosen in (("metre", (1.0,), names), ("gram", (1.0,), some),
                             ("second", (700.0,), some),
                             ("pascal_unit", (1.0,), some)):
        a = byid[uid]
        for pa in chosen:
            for pb in chosen:
                if pa == pb:
                    continue
                va = scale_value(a, prefix_exp.get(pa, 0))
                vb = scale_value(a, prefix_exp.get(pb, 0))
                for x in ins:
                    e, has_pi = _expected(x, va, 0, vb, 0)
                    convs.append((uid, pa, uid, pb, x, e, False))
    w("static oracle_conversion_t const oracle_conversions[] = {")
    for f, fp, t, tp, x, e, has_pi in convs:
        w("  { %s, %s, %s, %s, %r, %r, %s }," % (
            cpp_literal(f), cpp_literal(fp), cpp_literal(t), cpp_literal(tp),
            x, e, "true" if has_pi else "false"))
    w("};")
    w("")
    # The exact ratio of every pair of units of one kind (prefix none) in the
    # canonical form, and the correctly rounded value of every allowed (unit,
    # prefix) in the coherent SI unit.
    w("struct oracle_ratio_t")
    w("{")
    w("  char const *from;")
    w("  char const *to;")
    w("  int k;")
    w("  long long p;")
    w("  long long q;")
    w("  int pi;")
    w("};")
    w("")
    w("struct oracle_value_t")
    w("{")
    w("  char const *unit;")
    w("  char const *prefix;")
    w("  double value;")
    w("};")
    w("")
    w("static oracle_ratio_t const oracle_ratios[] = {")
    for kind, units in kinds.items():
        for a in units:
            for b in units:
                if a is b:
                    continue
                k, p, q = canon(a["value"] / b["value"])
                w("  { %s, %s, %d, %d, %d, %d }," % (
                    cpp_literal(a["id"]), cpp_literal(b["id"]), k, p, q,
                    a["pi"] - b["pi"]))
    w("};")
    w("")
    w("static oracle_value_t const oracle_values[] = {")
    for r in R:
        if r["factor"] == "none":
            continue
        masks = [("none", 0)]
        if r["prefixes"] == "all":
            masks += [(n, e) for n, e, _, _, _ in PREFIXES]
        elif r["prefixes"] != "none":
            masks += [(n, e) for n, e, _, _, _ in PREFIXES
                      if n in r["prefixes"].split("|")]
        for name, exp in masks:
            value, _ = _expected(1.0, r["value"] * Fraction(10) ** exp,
                                 r["pi"], Fraction(1), 0)
            w("  { %s, %s, %r }," % (cpp_literal(r["id"]), cpp_literal(name),
                                     value))
    w("};")
    w("")
    items, sweep = sweep_data()
    w("// The sweep: x * ratio of two (unit, prefix) items of one quantity")
    w("// kind, as the bit pattern of the correctly rounded double. The four")
    w("// inputs are the entries of oracle_sweep_inputs selected by the four")
    w("// nibbles of `selection`, the lowest first.")
    w("struct oracle_sweep_item_t")
    w("{")
    w("  char const *unit;")
    w("  char const *prefix;")
    w("};")
    w("")
    w("struct oracle_sweep_t")
    w("{")
    w("  unsigned short from;")
    w("  unsigned short to;")
    w("  unsigned short selection;")
    w("  unsigned long long expected[4];")
    w("};")
    w("")
    w("static double const oracle_sweep_inputs[] = {")
    for x in SWEEP_INPUTS:
        w("  %r," % x)
    w("};")
    w("")
    w("static oracle_sweep_item_t const oracle_sweep_items[] = {")
    for uid, prefix in items:
        w("  { %s, %s }," % (cpp_literal(uid), cpp_literal(prefix)))
    w("};")
    w("")
    w("static oracle_sweep_t const oracle_sweeps[] = {")
    for fi, ti, selection, expected in sweep:
        w("  { %d, %d, 0x%04x, { %s } }," % (
            fi, ti, selection,
            ", ".join("0x%016Xull" % e for e in expected)))
    w("};")
    w("")
    w("static std::size_t const oracle_sweep_count")
    w("    = sizeof (oracle_sweeps) / sizeof (oracle_sweeps[0]);")
    w("static std::size_t const oracle_sweep_item_count")
    w("    = sizeof (oracle_sweep_items) / sizeof (oracle_sweep_items[0]);")
    w("static std::size_t const oracle_row_count")
    w("    = sizeof (oracle_rows) / sizeof (oracle_rows[0]);")
    w("static std::size_t const oracle_prefix_count")
    w("    = sizeof (oracle_prefixes) / sizeof (oracle_prefixes[0]);")
    w("static std::size_t const oracle_ratio_count")
    w("    = sizeof (oracle_ratios) / sizeof (oracle_ratios[0]);")
    w("static std::size_t const oracle_value_count")
    w("    = sizeof (oracle_values) / sizeof (oracle_values[0]);")
    w("static std::size_t const oracle_conversion_count")
    w("    = sizeof (oracle_conversions) / sizeof (oracle_conversions[0]);")
    w("} // namespace lumex_test_units")
    w("")
    w("#endif // LUMEX_TESTS_CORE_UNITS_ORACLE_DATA_HPP")
    w("// clang-format on")
    return "\n".join(out) + "\n"


def emit_cpp():
    sys.stdout.write(cpp_text())


def check_cpp(path):
    """Exit status 1 when the file differs from what --cpp writes (line
    endings do not count: a checkout may have CRLF)."""
    with open(path, encoding="utf-8", newline=None) as handle:
        have = handle.read()
    if have != cpp_text():
        sys.stderr.write("%s is not what oracle_units.py --cpp writes; "
                         "regenerate it\n" % path)
        return 1
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    if "--cpp" in sys.argv:
        emit_cpp()
    elif "--check" in sys.argv:
        sys.exit(check_cpp(sys.argv[sys.argv.index("--check") + 1]))
    else:
        main()

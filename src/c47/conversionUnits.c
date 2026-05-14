// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file conversionUnits.c
 ***********************************************/

#include "c47.h"

#define inverting true
#define noninverting false


typedef struct {
  int16_t item;                                                                  // sort key — the item number to look up
  int16_t partner;                                                               // the paired item number
} convPair_t;

//==============================================================================
// ADD A NEW CONVERSION PAIR TO THE PAIR-LIST
//==============================================================================
// 1. Add the pair to the matching menu_Conv* array in items.h. Two adjacent ITM_ entries in a row form one pair (positions 0/1, 2/3, 4/5, ...).
//
// 2. Add TWO rows below, bidirectional
//      { ITM_AtoB , ITM_BtoA },   // NNN <-> MMM
//      { ITM_BtoA , ITM_AtoB },   // MMM <-> NNN
//
// 3. KEEP THE ARRAY SORTED ASCENDING by the FIRST item code. The binary search in isOneOfAConvertPair() silently fails on unsorted entries.
//
// 4. Update NUM_CONVERT_PAIRS by 2.
//==============================================================================

#define NUM_CONVERT_PAIRS  260

TO_QSPI const convPair_t convertPairs[NUM_CONVERT_PAIRS] = {                     // sorted by item
  { ITM_CtoF         , ITM_FtoC         },  //  220 <->  221
  { ITM_FtoC         , ITM_CtoF         },  //  221 <->  220
  { ITM_DBtoPR       , ITM_PRtoDB       },  //  222 <->  228
  { ITM_FT2toHA      , ITM_HAtoFT2      },  //  223 <->  224
  { ITM_HAtoFT2      , ITM_FT2toHA      },  //  224 <->  223
  { ITM_DBtoFR       , ITM_FRtoDB       },  //  225 <->  231
  { ITM_FT2toM2      , ITM_M2toFT2      },  //  226 <->  227
  { ITM_M2toFT2      , ITM_FT2toM2      },  //  227 <->  226
  { ITM_PRtoDB       , ITM_DBtoPR       },  //  228 <->  222
  { ITM_HAtoKM2      , ITM_KM2toHA      },  //  229 <->  230
  { ITM_KM2toHA      , ITM_HAtoKM2      },  //  230 <->  229
  { ITM_FRtoDB       , ITM_DBtoFR       },  //  231 <->  225
  { ITM_GLUKtoFZUK   , ITM_FZUKtoGLUK   },  //  232 <->  233
  { ITM_FZUKtoGLUK   , ITM_GLUKtoFZUK   },  //  233 <->  232
  { ITM_ACtoHA       , ITM_HAtoAC       },  //  234 <->  236
  { ITM_MLtoIN3      , ITM_IN3toML      },  //  235 <->  237
  { ITM_HAtoAC       , ITM_ACtoHA       },  //  236 <->  234
  { ITM_IN3toML      , ITM_MLtoIN3      },  //  237 <->  235
  { ITM_ACUStoHA     , ITM_HAtoACUS     },  //  238 <->  240
  { ITM_FT3toGLUK    , ITM_GLUKtoFT3    },  //  239 <->  241
  { ITM_HAtoACUS     , ITM_ACUStoHA     },  //  240 <->  238
  { ITM_GLUKtoFT3    , ITM_FT3toGLUK    },  //  241 <->  239
  { ITM_PAtoATM      , ITM_ATMtoPA      },  //  242 <->  243
  { ITM_ATMtoPA      , ITM_PAtoATM      },  //  243 <->  242
  { ITM_AUtoM        , ITM_MtoAU        },  //  244 <->  245
  { ITM_MtoAU        , ITM_AUtoM        },  //  245 <->  244
  { ITM_BARtoPA      , ITM_PAtoBAR      },  //  246 <->  247
  { ITM_PAtoBAR      , ITM_BARtoPA      },  //  247 <->  246
  { ITM_BTUtoJ       , ITM_JtoBTU       },  //  248 <->  249
  { ITM_JtoBTU       , ITM_BTUtoJ       },  //  249 <->  248
  { ITM_CALtoJ       , ITM_JtoCAL       },  //  250 <->  251
  { ITM_JtoCAL       , ITM_CALtoJ       },  //  251 <->  250
  { ITM_LBFFTtoNM    , ITM_NMtoLBFFT    },  //  252 <->  254
  { ITM_LtoFT3       , ITM_FT3toL       },  //  253 <->  255
  { ITM_NMtoLBFFT    , ITM_LBFFTtoNM    },  //  254 <->  252
  { ITM_FT3toL       , ITM_LtoFT3       },  //  255 <->  253
  { ITM_CWTtoKG      , ITM_KGtoCWT      },  //  256 <->  257
  { ITM_KGtoCWT      , ITM_CWTtoKG      },  //  257 <->  256
  { ITM_FTtoM        , ITM_MtoFT        },  //  258 <->  259
  { ITM_MtoFT        , ITM_FTtoM        },  //  259 <->  258
  { ITM_FTUStoM      , ITM_MtoFTUS      },  //  260 <->  263
  { ITM_LtoQTUS      , ITM_QTUStoL      },  //  261 <->  262
  { ITM_QTUStoL      , ITM_LtoQTUS      },  //  262 <->  261
  { ITM_MtoFTUS      , ITM_FTUStoM      },  //  263 <->  260
  { ITM_IN3toFZUK    , ITM_FZUKtoIN3    },  //  264 <->  265
  { ITM_FZUKtoIN3    , ITM_IN3toFZUK    },  //  265 <->  264
  { ITM_FZUKtoML     , ITM_MLtoFZUK     },  //  266 <->  268
  { ITM_IN3toFZUS    , ITM_FZUStoIN3    },  //  267 <->  269
  { ITM_MLtoFZUK     , ITM_FZUKtoML     },  //  268 <->  266
  { ITM_FZUStoIN3    , ITM_IN3toFZUS    },  //  269 <->  267
  { ITM_FZUStoML     , ITM_MLtoFZUS     },  //  270 <->  272
  { ITM_FT3toGalUS   , ITM_GalUStoFT3   },  //  271 <->  273
  { ITM_MLtoFZUS     , ITM_FZUStoML     },  //  272 <->  270
  { ITM_GalUStoFT3   , ITM_FT3toGalUS   },  //  273 <->  271
  { ITM_GLUKtoL      , ITM_LtoGLUK      },  //  274 <->  275
  { ITM_LtoGLUK      , ITM_GLUKtoL      },  //  275 <->  274
  { ITM_GLUStoL      , ITM_LtoGLUS      },  //  276 <->  277
  { ITM_LtoGLUS      , ITM_GLUStoL      },  //  277 <->  276
  { ITM_HPEtoW       , ITM_WtoHPE       },  //  278 <->  279
  { ITM_WtoHPE       , ITM_HPEtoW       },  //  279 <->  278
  { ITM_HPMtoW       , ITM_WtoHPM       },  //  280 <->  281
  { ITM_WtoHPM       , ITM_HPMtoW       },  //  281 <->  280
  { ITM_HPUKtoW      , ITM_WtoHPUK      },  //  282 <->  283
  { ITM_WtoHPUK      , ITM_HPUKtoW      },  //  283 <->  282
  { ITM_INCHHGtoPA   , ITM_PAtoINCHHG   },  //  284 <->  286
  { ITM_CUPCtoFZUS   , ITM_FZUStoCUPC   },  //  285 <->  306
  { ITM_PAtoINCHHG   , ITM_INCHHGtoPA   },  //  286 <->  284
  { ITM_CUPCtoML     , ITM_MLtoCUPC     },  //  287 <->  312
  { ITM_INCHtoMM     , ITM_MMtoINCH     },  //  288 <->  289
  { ITM_MMtoINCH     , ITM_INCHtoMM     },  //  289 <->  288
  { ITM_WHtoJ        , ITM_JtoWH        },  //  290 <->  291
  { ITM_JtoWH        , ITM_WHtoJ        },  //  291 <->  290
  { ITM_KGtoLBS      , ITM_LBStoKG      },  //  292 <->  293
  { ITM_LBStoKG      , ITM_KGtoLBS      },  //  293 <->  292
  { ITM_GtoOZ        , ITM_OZtoG        },  //  294 <->  295
  { ITM_OZtoG        , ITM_GtoOZ        },  //  295 <->  294
  { ITM_KGtoSCW      , ITM_SCWtoKG      },  //  296 <->  298
  { ITM_CUPUKtoFZUK  , ITM_FZUKtoCUPUK  },  //  297 <->  301
  { ITM_SCWtoKG      , ITM_KGtoSCW      },  //  298 <->  296
  { ITM_CUPUKtoML    , ITM_MLtoCUPUK    },  //  299 <->  315
  { ITM_KGtoSTO      , ITM_STOtoKG      },  //  300 <->  302
  { ITM_FZUKtoCUPUK  , ITM_CUPUKtoFZUK  },  //  301 <->  297
  { ITM_STOtoKG      , ITM_KGtoSTO      },  //  302 <->  300
  { ITM_FZUKtoTBSPUK , ITM_TBSPUKtoFZUK },  //  303 <->  383
  { ITM_KGtoST       , ITM_STtoKG       },  //  304 <->  307
  { ITM_FZUKtoTSPUK  , ITM_TSPUKtoFZUK  },  //  305 <->  394
  { ITM_FZUStoCUPC   , ITM_CUPCtoFZUS   },  //  306 <->  285
  { ITM_STtoKG       , ITM_KGtoST       },  //  307 <->  304
  { ITM_FZUStoTBSPC  , ITM_TBSPCtoFZUS  },  //  308 <->  365
  { ITM_FZUStoTSPC   , ITM_TSPCtoFZUS   },  //  309 <->  392
  { ITM_KGtoTON      , ITM_TONtoKG      },  //  310 <->  313
  { ITM_KGtoLIANG    , ITM_LIANGtoKG    },  //  311 <->  314
  { ITM_MLtoCUPC     , ITM_CUPCtoML     },  //  312 <->  287
  { ITM_TONtoKG      , ITM_KGtoTON      },  //  313 <->  310
  { ITM_LIANGtoKG    , ITM_KGtoLIANG    },  //  314 <->  311
  { ITM_MLtoCUPUK    , ITM_CUPUKtoML    },  //  315 <->  299
  { ITM_GtoTRZ       , ITM_TRZtoG       },  //  316 <->  318
  { ITM_MLtoPINTLQ   , ITM_PINTLQtoML   },  //  317 <->  351
  { ITM_TRZtoG       , ITM_GtoTRZ       },  //  318 <->  316
  { ITM_MLtoPINTUK   , ITM_PINTUKtoML   },  //  319 <->  354
  { ITM_LBFtoN       , ITM_NtoLBF       },  //  320 <->  321
  { ITM_NtoLBF       , ITM_LBFtoN       },  //  321 <->  320
  { ITM_LYtoM        , ITM_MtoLY        },  //  322 <->  323
  { ITM_MtoLY        , ITM_LYtoM        },  //  323 <->  322
  { ITM_MMHGtoPA     , ITM_PAtoMMHG     },  //  324 <->  326
  { ITM_MLtoQT       , ITM_QTtoML       },  //  325 <->  359
  { ITM_PAtoMMHG     , ITM_MMHGtoPA     },  //  326 <->  324
  { ITM_MLtoQTUS     , ITM_QTUStoML     },  //  327 <->  362
  { ITM_MItoKM       , ITM_KMtoMI       },  //  328 <->  329
  { ITM_KMtoMI       , ITM_MItoKM       },  //  329 <->  328
  { ITM_KMtoNMI      , ITM_NMItoKM      },  //  330 <->  331
  { ITM_NMItoKM      , ITM_KMtoNMI      },  //  331 <->  330
  { ITM_MtoPC        , ITM_PCtoM        },  //  332 <->  333
  { ITM_PCtoM        , ITM_MtoPC        },  //  333 <->  332
  { ITM_MMtoPOINT    , ITM_POINTtoMM    },  //  334 <->  337
  { ITM_MLtoTBSPC    , ITM_TBSPCtoML    },  //  335 <->  369
  { ITM_MILEtoM      , ITM_MtoMILE      },  //  336 <->  339
  { ITM_POINTtoMM    , ITM_MMtoPOINT    },  //  337 <->  334
  { ITM_MLtoTBSPUK   , ITM_TBSPUKtoML   },  //  338 <->  385
  { ITM_MtoMILE      , ITM_MILEtoM      },  //  339 <->  336
  { ITM_MtoYD        , ITM_YDtoM        },  //  340 <->  341
  { ITM_YDtoM        , ITM_MtoYD        },  //  341 <->  340
  { ITM_PSItoPA      , ITM_PAtoPSI      },  //  342 <->  343
  { ITM_PAtoPSI      , ITM_PSItoPA      },  //  343 <->  342
  { ITM_PAtoTOR      , ITM_TORtoPA      },  //  344 <->  346
  { ITM_MLtoTSPC     , ITM_TSPCtoML     },  //  345 <->  393
  { ITM_TORtoPA      , ITM_PAtoTOR      },  //  346 <->  344
  { ITM_MLtoTSPUK    , ITM_TSPUKtoML    },  //  347 <->  395
  { ITM_StoYEAR      , ITM_YEARtoS      },  //  348 <->  349
  { ITM_YEARtoS      , ITM_StoYEAR      },  //  349 <->  348
  { ITM_CARATtoG     , ITM_GtoCARAT     },  //  350 <->  353
  { ITM_PINTLQtoML   , ITM_MLtoPINTLQ   },  //  351 <->  317
  { ITM_JINtoKG      , ITM_KGtoJIN      },  //  352 <->  355
  { ITM_GtoCARAT     , ITM_CARATtoG     },  //  353 <->  350
  { ITM_PINTUKtoML   , ITM_MLtoPINTUK   },  //  354 <->  319
  { ITM_KGtoJIN      , ITM_JINtoKG      },  //  355 <->  352
  { ITM_QTtoL        , ITM_LtoQT        },  //  356 <->  357
  { ITM_LtoQT        , ITM_QTtoL        },  //  357 <->  356
  { ITM_FATHOMtoM    , ITM_MtoFATHOM    },  //  358 <->  361
  { ITM_QTtoML       , ITM_MLtoQT       },  //  359 <->  325
  { ITM_NMItoM       , ITM_MtoNMI       },  //  360 <->  363
  { ITM_MtoFATHOM    , ITM_FATHOMtoM    },  //  361 <->  358
  { ITM_QTUStoML     , ITM_MLtoQTUS     },  //  362 <->  327
  { ITM_MtoNMI       , ITM_NMItoM       },  //  363 <->  360
  { ITM_BARRELtoM3   , ITM_M3toBARREL   },  //  364 <->  366
  { ITM_TBSPCtoFZUS  , ITM_FZUStoTBSPC  },  //  365 <->  308
  { ITM_M3toBARREL   , ITM_BARRELtoM3   },  //  366 <->  364
  { ITM_HMStoHR      , ITM_HRtoHMS      },  //  367 <->  368
  { ITM_HRtoHMS      , ITM_HMStoHR      },  //  368 <->  367
  { ITM_TBSPCtoML    , ITM_MLtoTBSPC    },  //  369 <->  335
  { ITM_HECTAREtoM2  , ITM_M2toHECTARE  },  //  370 <->  371
  { ITM_M2toHECTARE  , ITM_HECTAREtoM2  },  //  371 <->  370
  { ITM_MUtoM2       , ITM_M2toMU       },  //  372 <->  373
  { ITM_M2toMU       , ITM_MUtoM2       },  //  373 <->  372
  { ITM_LItoM        , ITM_MtoLI        },  //  374 <->  375
  { ITM_MtoLI        , ITM_LItoM        },  //  375 <->  374
  { ITM_CHItoM       , ITM_MtoCHI       },  //  376 <->  377
  { ITM_MtoCHI       , ITM_CHItoM       },  //  377 <->  376
  { ITM_YINtoM       , ITM_MtoYIN       },  //  378 <->  379
  { ITM_MtoYIN       , ITM_YINtoM       },  //  379 <->  378
  { ITM_CUNtoM       , ITM_MtoCUN       },  //  380 <->  381
  { ITM_MtoCUN       , ITM_CUNtoM       },  //  381 <->  380
  { ITM_ZHANGtoM     , ITM_MtoZHANG     },  //  382 <->  384
  { ITM_TBSPUKtoFZUK , ITM_FZUKtoTBSPUK },  //  383 <->  303
  { ITM_MtoZHANG     , ITM_ZHANGtoM     },  //  384 <->  382
  { ITM_TBSPUKtoML   , ITM_MLtoTBSPUK   },  //  385 <->  338
  { ITM_FENtoM       , ITM_MtoFEN       },  //  386 <->  387
  { ITM_MtoFEN       , ITM_FENtoM       },  //  387 <->  386
  { ITM_MI2toKM2     , ITM_KM2toMI2     },  //  388 <->  389
  { ITM_KM2toMI2     , ITM_MI2toKM2     },  //  389 <->  388
  { ITM_NMI2toKM2    , ITM_KM2toNMI2    },  //  390 <->  391
  { ITM_KM2toNMI2    , ITM_NMI2toKM2    },  //  391 <->  390
  { ITM_TSPCtoFZUS   , ITM_FZUStoTSPC   },  //  392 <->  309
  { ITM_TSPCtoML     , ITM_MLtoTSPC     },  //  393 <->  345
  { ITM_TSPUKtoFZUK  , ITM_FZUKtoTSPUK  },  //  394 <->  305
  { ITM_TSPUKtoML    , ITM_MLtoTSPUK    },  //  395 <->  347
  { ITM_GLUStoFZUS   , ITM_FZUStoGLUS   },  // 1902 <-> 1903
  { ITM_FZUStoGLUS   , ITM_GLUStoFZUS   },  // 1903 <-> 1902
  { ITM_KNOTtoKMH    , ITM_KMHtoKNOT    },  // 2084 <-> 2085
  { ITM_KMHtoKNOT    , ITM_KNOTtoKMH    },  // 2085 <-> 2084
  { ITM_KMHtoMPS     , ITM_MPStoKMH     },  // 2086 <-> 2087
  { ITM_MPStoKMH     , ITM_KMHtoMPS     },  // 2087 <-> 2086
  { ITM_RPMtoDEGPS   , ITM_DEGPStoRPM   },  // 2088 <-> 2089
  { ITM_DEGPStoRPM   , ITM_RPMtoDEGPS   },  // 2089 <-> 2088
  { ITM_MPHtoKMH     , ITM_KMHtoMPH     },  // 2090 <-> 2091
  { ITM_KMHtoMPH     , ITM_MPHtoKMH     },  // 2091 <-> 2090
  { ITM_MPHtoMPS     , ITM_MPStoMPH     },  // 2092 <-> 2093
  { ITM_MPStoMPH     , ITM_MPHtoMPS     },  // 2093 <-> 2092
  { ITM_RPMtoRADPS   , ITM_RADPStoRPM   },  // 2094 <-> 2095
  { ITM_RADPStoRPM   , ITM_RPMtoRADPS   },  // 2095 <-> 2094
  { ITM_DEGtoRAD     , ITM_RADtoDEG     },  // 2096 <-> 2097
  { ITM_RADtoDEG     , ITM_DEGtoRAD     },  // 2097 <-> 2096
  { ITM_DEGtoGRAD    , ITM_GRADtoDEG    },  // 2098 <-> 2099
  { ITM_GRADtoDEG    , ITM_DEGtoGRAD    },  // 2099 <-> 2098
  { ITM_GRADtoRAD    , ITM_RADtoGRAD    },  // 2100 <-> 2101
  { ITM_RADtoGRAD    , ITM_GRADtoRAD    },  // 2101 <-> 2100
  { ITM_INCHtoCM     , ITM_CMtoINCH     },  // 2163 <-> 2164
  { ITM_CMtoINCH     , ITM_INCHtoCM     },  // 2164 <-> 2163
  { ITM_NMItoMI      , ITM_MItoNMI      },  // 2167 <-> 2168
  { ITM_MItoNMI      , ITM_NMItoMI      },  // 2168 <-> 2167
  { ITM_FURtoM       , ITM_MtoFUR       },  // 2169 <-> 2170
  { ITM_MtoFUR       , ITM_FURtoM       },  // 2170 <-> 2169
  { ITM_FTNtoS       , ITM_StoFTN       },  // 2171 <-> 2172
  { ITM_StoFTN       , ITM_FTNtoS       },  // 2172 <-> 2171
  { ITM_FPFtoMPS     , ITM_MPStoFPF     },  // 2173 <-> 2174
  { ITM_MPStoFPF     , ITM_FPFtoMPS     },  // 2174 <-> 2173
  { ITM_BRDStoM      , ITM_MtoBRDS      },  // 2175 <-> 2176
  { ITM_MtoBRDS      , ITM_BRDStoM      },  // 2176 <-> 2175
  { ITM_FIRtoKG      , ITM_KGtoFIR      },  // 2177 <-> 2178
  { ITM_KGtoFIR      , ITM_FIRtoKG      },  // 2178 <-> 2177
  { ITM_FPFtoKPH     , ITM_KPHtoFPF     },  // 2179 <-> 2180
  { ITM_KPHtoFPF     , ITM_FPFtoKPH     },  // 2180 <-> 2179
  { ITM_BRDStoIN     , ITM_INtoBRDS     },  // 2181 <-> 2182
  { ITM_INtoBRDS     , ITM_BRDStoIN     },  // 2182 <-> 2181
  { ITM_FIRtoLB      , ITM_LBtoFIR      },  // 2183 <-> 2184
  { ITM_LBtoFIR      , ITM_FIRtoLB      },  // 2184 <-> 2183
  { ITM_FPFtoMPH     , ITM_MPHtoFPF     },  // 2185 <-> 2186
  { ITM_MPHtoFPF     , ITM_FPFtoMPH     },  // 2186 <-> 2185
  { ITM_FPStoKMH     , ITM_KMHtoFPS     },  // 2187 <-> 2188
  { ITM_KMHtoFPS     , ITM_FPStoKMH     },  // 2188 <-> 2187
  { ITM_FPStoMPS     , ITM_MPStoFPS     },  // 2189 <-> 2190
  { ITM_MPStoFPS     , ITM_FPStoMPS     },  // 2190 <-> 2189
  { ITM_L100toKML    , ITM_KMLtoL100    },  // 2204 <-> 2205
  { ITM_KMLtoL100    , ITM_L100toKML    },  // 2205 <-> 2204
  { ITM_KMLEtoK100K  , ITM_K100KtoKMLE  },  // 2206 <-> 2207
  { ITM_K100KtoKMLE  , ITM_KMLEtoK100K  },  // 2207 <-> 2206
  { ITM_K100KtoKMK   , ITM_KMKtoK100K   },  // 2208 <-> 2209
  { ITM_KMKtoK100K   , ITM_K100KtoKMK   },  // 2209 <-> 2208
  { ITM_L100toMGUS   , ITM_MGUStoL100   },  // 2210 <-> 2211
  { ITM_MGUStoL100   , ITM_L100toMGUS   },  // 2211 <-> 2210
  { ITM_MGEUStoK100M , ITM_K100MtoMGEUS },  // 2212 <-> 2213
  { ITM_K100MtoMGEUS , ITM_MGEUStoK100M },  // 2213 <-> 2212
  { ITM_K100KtoK100M , ITM_K100MtoK100K },  // 2214 <-> 2215
  { ITM_K100MtoK100K , ITM_K100KtoK100M },  // 2215 <-> 2214
  { ITM_L100toMGUK   , ITM_MGUKtoL100   },  // 2216 <-> 2217
  { ITM_MGUKtoL100   , ITM_L100toMGUK   },  // 2217 <-> 2216
  { ITM_MGEUKtoK100M , ITM_K100MtoMGEUK },  // 2218 <-> 2219
  { ITM_K100MtoMGEUK , ITM_MGEUKtoK100M },  // 2219 <-> 2218
  { ITM_K100MtoMIK   , ITM_MIKtoK100M   },  // 2220 <-> 2221
  { ITM_MIKtoK100M   , ITM_K100MtoMIK   },  // 2221 <-> 2220
  { ITM_EVtoJ        , ITM_JtoEV        },  // 2464 <-> 2465
  { ITM_JtoEV        , ITM_EVtoJ        },  // 2465 <-> 2464
  { ITM_BANANAtoINCH , ITM_INCHtoBANANA },  // 2466 <-> 2467
  { ITM_INCHtoBANANA , ITM_BANANAtoINCH },  // 2467 <-> 2466
  { ITM_BANANAtoMM   , ITM_MMtoBANANA   },  // 2468 <-> 2469
  { ITM_MMtoBANANA   , ITM_BANANAtoMM   },  // 2469 <-> 2468
  { ITM_ERGtoJ       , ITM_JtoERG       },  // 2658 <-> 2659
  { ITM_JtoERG       , ITM_ERGtoJ       },  // 2659 <-> 2658
  { ITM_FoetoJ       , ITM_JtoFoe       },  // 2660 <-> 2661
  { ITM_JtoFoe       , ITM_FoetoJ       },  // 2661 <-> 2660
  { ITM_CtoK         , ITM_KtoC         },  // 2665 <-> 2666
  { ITM_KtoC         , ITM_CtoK         },  // 2666 <-> 2665
  { ITM_RAtoK        , ITM_KtoRA        },  // 2667 <-> 2668
  { ITM_KtoRA        , ITM_RAtoK        },  // 2668 <-> 2667
  { ITM_RAtoF        , ITM_FtoRA        },  // 2669 <-> 2670
  { ITM_FtoRA        , ITM_RAtoF        },  // 2670 <-> 2669
  { ITM_EVKBtoK      , ITM_KtoEVKB      },  // 2671 <-> 2672
  { ITM_KtoEVKB      , ITM_EVKBtoK      },  // 2672 <-> 2671
  { ITM_FtoK         , ITM_KtoF         },  // 2673 <-> 2674
  { ITM_KtoF         , ITM_FtoK         },  // 2674 <-> 2673
};

static int16_t partner(int16_t input) {
  uint16_t lo = 0;                                                               // binary search bounds
  uint16_t hi = NUM_CONVERT_PAIRS;
  while(lo < hi) {                                                               // find first .item >= input
    const uint16_t mid = (lo + hi) >> 1;
    if(convertPairs[mid].item < input) {
      lo = mid + 1;
    }
    else {
      hi = mid;
    }
  }
  if(lo == NUM_CONVERT_PAIRS || convertPairs[lo].item != input) {
    return 0;                                                                    // not found
  }
  return convertPairs[lo].partner;
}

bool_t isOneOfAConvertPair(uint16_t x, int16_t itemNr, int16_t *oddNrPartner) {
  const int16_t p = partner(itemNr);
  if(p == 0) {
    return false;                                                                // not a conversion-pair member
  }
  if((x & 1) == 0) {
    *oddNrPartner = p;                                                           // even x = left softkey: report partner
  }
  return true;
}


static void unitConversion(const real_t * const coefficient, uint16_t multiplyDivide, bool_t invert) {
  real_t reX;

  if(!getRegisterAsReal(REGISTER_X, &reX)) {
    return;
  }

  if(!saveLastX()) {
    return;
  }

  if(invert && realIsZero(&reX)) {
    if(getSystemFlag(FLAG_SPCRES)) {
      convertRealToResultRegister(realIsNegative(&reX) ? const_minusInfinity : const_plusInfinity, REGISTER_X, amNone);
    }
    else {
      displayCalcErrorMessage(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN, ERR_REGISTER_LINE, REGISTER_X);
      #if (EXTRA_INFO_ON_CALC_ERROR == 1)
        moreInfoOnError("In function unitConversion:", "cannot calculate divide by zero", NULL, NULL);
      #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    }
  }

  if(invert) {
    realDivide(const_1, &reX, &reX, &ctxtReal39);
  }

  if(multiplyDivide == multiply) {
    realMultiply(&reX, coefficient, &reX, &ctxtReal39);
  }
  else {
    realDivide(&reX, coefficient, &reX, &ctxtReal39);
  }

  convertRealToResultRegister(&reX, REGISTER_X, amNone);
  adjustResult(REGISTER_X, false, false, -1, -1, -1);
}

TO_QSPI static const real_t * const conversionFactors[constFactorEND] = {
    [constFactorFt2Hectare]   = const_Ft2ToHa,      /*   0 */
    [constFactorFt2M2]        = const_Ft2ToM2,
    [constFactorHectareKm2]   = const_100,
    [constFactorAcreHa]       = const_AccreToHa,
    [constFactorAcreusHa]     = const39_AccreusToHa,
    [constFactorAtmPa]        = const_AtmToPa,
    [constFactorAuM]          = const_AuToM,
    [constFactorBarPa]        = const_BarToPa,
    [constFactorBtuJ]         = const_BtuToJ,
    [constFactorCalJ]         = const_CalToJ,
    [constFactorLbfftNm]      = const_LbfftToNm,    /*  10 */
    [constFactorCwtKg]        = const_CwtToKg,
    [constFactorFtM]          = const_FtToM,
    [constFactorSfeetM]       = const39_SfeetToM,
    [constFactorFlozukIn3]    = const_FlozukToIn3,
    [constFactorFlozukMl]     = const_FlozukToMl,
    [constFactorFlozusIn3]    = const_FlozusToIn3,
    [constFactorFlozusMl]     = const_FlozusToMl,
    [constFactorFt3toGalUS]   = const_Ft3ToGalUS,
    [constFactorGalukL]       = const_GalukToL,
    [constFactorGalusL]       = const_GalusToL,     /*  20 */
    [constFactorHpeW]         = const_HpeToW,
    [constFactorHpmW]         = const_HpmToW,
    [constFactorHpukW]        = const_HpukToW,
    [constFactorInhgPa]       = const_InhgToPa,
    [constFactorInchMm]       = const_InchToMm,
    [constFactorWhJ]          = const_WhToJ,
    [constFactorLbKg]         = const_LbToKg,
    [constFactorOzG]          = const_OzToG,
    [constFactorShortcwtKg]   = const_ShortcwtToKg,
    [constFactorStoneKg]      = const_StoneToKg,    /*  30 */
    [constFactorShorttonKg]   = const_ShorttonToKg,
    [constFactorTonKg]        = const_TonToKg,
    [constFactorLiangKg]      = const_20,
    [constFactorTrozG]        = const_TrozToG,
    [constFactorLbfN]         = const_LbfToN,
    [constFactorLyM]          = const_LyToM,
    [constFactorMmhgPa]       = const_MmhgToPa,
    [constFactorMiKm]         = const_MiToKm,
    [constFactorNmiKm]        = const_NmiToKm,
    [constFactorPcM]          = const39_PcToM,      /*  40 */
    [constFactorPointMm]      = const39_PointToMm,
    [constFactorMileM]        = const_MiToM,
    [constFactorYardM]        = const_YardToM,
    [constFactorPsiPa]        = const39_PsiToPa,
    [constFactorTorrPa]       = const39_TorrToPa,
    [constFactorYearS]        = const_YearToS,
    [constFactorCaratG]       = const_CaratToG,
    [constFactorJinKg]        = const_2,
    [constFactorQuartL]       = const_QuartToL,
    [constFactorFathomM]      = const_FathomToM,    /*  50 */
    [constFactorNMiM]         = const_NmiToM,
    [constFactorBarrelM3]     = const_BarrelToM3,
    [constFactorHectareM2]    = const_10000,
    [constFactorMuM2]         = const_MuToM2,
    [constFactorLiM]          = const_LiToM,
    [constFactorChiM]         = const_3,
    [constFactorYinM]         = const_YinToM,
    [constFactorCunM]         = const_CunToM,
    [constFactorZhangM]       = const_ZhangToM,
    [constFactorFenM]         = const_FenToM,       /*  60 */
    [constFactorMi2Km2]       = const_MiSqToKmSq,
    [constFactorNmi2Km2]      = const_NmiSqToKmSq,
    [constFactorKmphmps]      = const39_Kmphmps,
    [constFactorRpmDegps]     = const_RpmDegps,
    [constFactorMphmps]       = const_Mphmps,
    [constFactorRpmRadps]     = const39_RpmRadps,
    [constFactorInchCm]       = const_InchToCm,
    [constFactorNmiMi]        = const39_NmiToMi,
    [constFactorFurtom]       = const_furToM,
    [constFactorFtntos]       = const_ftnToS,       /*  70 */
    [constFactorFpftomps]     = const_fpfToMps,
    [constFactorBrdstom]      = const_brdsTom,
    [constFactorFirtokg]      = const_firToKg,
    [constFactorFpftokph]     = const_fpfToKph,
    [constFactorBrdstoin]     = const_brdsToIn,
    [constFactorFirtolb]      = const_firToLb,
    [constFactorFpftomph]     = const_fpfToMph,
    [constFactorFpstokph]     = const_fpsToKph,
    [constFactorFpstomps]     = const_fpsToMps,
    [constFactorL100Tokml]    = const_100,          /*  80 */
    [constFactorKmletok100K]  = NULL,
    [constFactorK100Ktokmk]   = const_100,
    [constFactorL100Tomgus]   = NULL,
    [constFactorMgeustok100M] = NULL,
    [constFactorK100Ktok100M] = const_MiToKm,
    [constFactorL100Tomguk]   = NULL,
    [constFactorMgeuktok100M] = NULL,
    [constFactorK100Mtomik]   = const_100,
    [constFactorCupcFzus]     = const_CupcFzus,
    [constFactorCupcMl]       = const_CupcMl,       /*  90 */
    [constFactorCupukFzuk]    = const_CupukFzuk,
    [constFactorCupukMl]      = const_CupukMl,
    [constFactorFzukCupuk]    = const_CupukFzuk,
    [constFactorFzukTbspuk]   = const_FzukTbspuk,
    [constFactorFzukTspuk]    = const_FzukTspuk,
    [constFactorFzusCupc]     = const_CupcFzus,
    [constFactorFzusTbspc]    = const_FzusTbspc,
    [constFactorFzusTspc]     = const_FzusTspc,
    [constFactorMlCupc]       = const_CupcMl,
    [constFactorMlCupuk]      = const_CupukMl,      /* 100 */
    [constFactorMlPintlq]     = const_PintlqMl,
    [constFactorMlPintuk]     = const_PintukMl,
    [constFactorMlQt]         = const_QtMl,
    [constFactorMlQtus]       = const_QtusMl,
    [constFactorMlTbspc]      = const_TbspcMl,
    [constFactorMlTbspuk]     = const_TbspukMl,
    [constFactorMlTspc]       = const_TspcMl,
    [constFactorMlTspuk]      = const39_TspukMl,
    [constFactorPintlqMl]     = const_PintlqMl,
    [constFactorPintukMl]     = const_PintukMl,     /* 110 */
    [constFactorQtMl]         = const_QtMl,
    [constFactorQtusMl]       = const_QtusMl,
    [constFactorTbspcFzus]    = const_FzusTbspc,
    [constFactorTbspcMl]      = const_TbspcMl,
    [constFactorTbspukFzuk]   = const_FzukTbspuk,
    [constFactorTbspukMl]     = const_TbspukMl,
    [constFactorTspcFzus]     = const_FzusTspc,
    [constFactorTspcMl]       = const_TspcMl,
    [constFactorTspukFzuk]    = const_FzukTspuk,
    [constFactorTspukMl]      = const39_TspukMl,    /* 120 */
    [constFactorMlIn3]        = const_In3Ml,
    [constFactorIn3Ml]        = const_In3Ml,
    [constFactorFt3Gluk]      = const_Ft3Gluk,
    [constFactorGlukFt3]      = const_Ft3Gluk,
    [constFactorLFt3]         = const_Ft3L,
    [constFactorFt3L]         = const_Ft3L,
    [constFactorLQtus]        = const_LQtus,
    [constFactorQtusL]        = const_LQtus,
    [constFactorGlukFzuk]     = const_GlukFzuk,
    [constFactorFzukGluk]     = const_GlukFzuk,     /* 130 */
    [constFactorGlusFzus]     = const_GlusFzus,
    [constFactorFzusGlus]     = const_GlusFzus,
    [constFactoreVJ]          = const_e,
    [constFactorJeV]          = const_e,
    [constFactormmBanana]     = const_bananamm,
    [constFactorBananamm]     = const_bananamm,
    [constFactorInchBanana]   = const39_bananaInch,
    [constFactorBananaInch]   = const39_bananaInch,
    [constFactorErgJ]         = const_ErgToJ,
    [constFactorFoeJ]         = const_FoeToJ,       /* 140 */
  };

void fnUnitConvert(uint16_t arg) {
    const uint16_t multiply = arg & 0x8000;
    const bool_t invert = (arg & 0x4000) != 0;
    const uint16_t idx = arg & 0x3fff;

    unitConversion(conversionFactors[idx], multiply, invert);
}


//  {[(x - B) / C] * D} + E
TO_QSPI static const real_t * const cvtTempConsts[13][4] = {
  //   B              C             D             E
  {const_0,       const_1,       const_9on5,    const_32     }, // ITM_CtoF     ix =  0
  {const_32,      const_9on5,    const_1,       const_0      }, // ITM_FtoC     ix =  1
  {const_0,       const_1,       const_1,       const_273p15 }, // ITM_CtoK     ix =  2
  {const_273p15,  const_1,       const_1,       const_0      }, // ITM_KtoC     ix =  3
  {const_0,       const_9on5,    const_1,       const_0      }, // ITM_RAtoK    ix =  4
  {const_0,       const_1,       const_9on5,    const_0      }, // ITM_KtoRA    ix =  5
  {const_459p67,  const_1,       const_1,       const_0      }, // ITM_RAtoF    ix =  6
  {const_0,       const_1,       const_1,       const_459p67 }, // ITM_FtoRA    ix =  7
  {const_0,       const39_kBeVK, const_1,       const_0      }, // ITM_EVKBtoK  ix =  8
  {const_0,       const_1,       const39_kBeVK, const_0      }, // ITM_KtoEVKB  ix =  9
  {const_32,      const_9on5,    const_1,       const_273p15 }, // ITM_FtoK     ix = 10
  {const_273p15,  const_1,       const_9on5,    const_32     }, // ITM_KtoF     ix = 11
};

void fnCvtTemp(uint16_t ix) {
  real_t reX;

  if(!getRegisterAsReal(REGISTER_X, &reX)) {
    return;
  }

  if(!saveLastX()) {
    return;
  }

  //  (x - B) / C * D + E

  //printf("ix = %d\n",ix);
  //printRealToConsole(cvtTempConsts[ix][0], "(x - "," ) \n");
  //printRealToConsole(cvtTempConsts[ix][1], "/ "," \n");
  //printRealToConsole(cvtTempConsts[ix][2], "x "," \n");
  //printRealToConsole(cvtTempConsts[ix][3], "+ ","\n");
  if(cvtTempConsts[ix][0] != const_0) {realSubtract(&reX, cvtTempConsts[ix][0], &reX, &ctxtReal39);}
  if(cvtTempConsts[ix][1] != const_1) {realDivide  (&reX, cvtTempConsts[ix][1], &reX, &ctxtReal39);}
  if(cvtTempConsts[ix][2] != const_1) {realMultiply(&reX, cvtTempConsts[ix][2], &reX, &ctxtReal39);}
  if(cvtTempConsts[ix][3] != const_0) {realAdd     (&reX, cvtTempConsts[ix][3], &reX, &ctxtReal39);}
  //printRealToConsole(&reX, "Rex: ","\n");

  convertRealToResultRegister(&reX, REGISTER_X, amNone);

  adjustResult(REGISTER_X, false, false, -1, -1, -1);
}


void fnCvtDegRad(uint16_t multiplyDivide) {
  if(getRegisterDataType(REGISTER_X) == dtReal34 && (
    ((getRegisterAngularMode(REGISTER_X) == amDegree) && multiplyDivide == divide) || ((getRegisterAngularMode(REGISTER_X) == amRadian) && multiplyDivide == multiply) )) {
    setRegisterAngularMode(REGISTER_X, amNone);
  }
  unitConversion(const39_180onPi, multiplyDivide, noninverting);
}

void fnCvtDegGrad(uint16_t multiplyDivide) {
  if(getRegisterDataType(REGISTER_X) == dtReal34 && (
    ((getRegisterAngularMode(REGISTER_X) == amDegree) && multiplyDivide == divide) || ((getRegisterAngularMode(REGISTER_X) == amGrad) && multiplyDivide == multiply) )) {
    setRegisterAngularMode(REGISTER_X, amNone);
  }
  unitConversion(const_9on10, multiplyDivide, noninverting);
}

void fnCvtGradRad(uint16_t multiplyDivide) {
  if(getRegisterDataType(REGISTER_X) == dtReal34 && (
    ((getRegisterAngularMode(REGISTER_X) == amGrad) && multiplyDivide == divide) || ((getRegisterAngularMode(REGISTER_X) == amRadian) && multiplyDivide == multiply) )) {
    setRegisterAngularMode(REGISTER_X, amNone);
  }
  unitConversion(const39_200onPi, multiplyDivide, noninverting);
}

void fnKmletok100K   (uint16_t multiplyDivide) {
  //note multiplyDivide is not used, as the formula is biderectional!
  //100*liter_equivalent  / (value), both ways
  real_t factor;
  realMultiply(const_GaluseqE, const_100, &factor, &ctxtReal39);
  realDivide(&factor, const_GalusToL, &factor, &ctxtReal39);
  unitConversion(&factor, multiply, inverting);
}

void fnL100Tomgus   (uint16_t multiplyDivide) {
  //note multiplyDivide is not used, as the formula is biderectional!
  //100 *gallon_US/mile   /  (value), both ways
  real_t factor;
  realMultiply(const_100, const_GalusToL, &factor, &ctxtReal39);
  realDivide(&factor, const_MiToKm, &factor, &ctxtReal39);
  unitConversion(&factor, multiply, inverting);
}

void fnMgeustok100M   (uint16_t multiplyDivide) {
  //note multiplyDivide is not used, as the formula is biderectional!
  //100*gallon_US_equivalent / (value), both ways
  real_t factor;
  realMultiply(const_GaluseqE, const_100, &factor, &ctxtReal39);
  unitConversion(&factor, multiply, inverting);
}

void fnL100Tomguk   (uint16_t multiplyDivide) {
  //note multiplyDivide is not used, as the formula is biderectional!
  //100*gallon_UK/mile  / (value), both ways
  real_t factor;
  realMultiply(const_100, const_GalukToL, &factor, &ctxtReal39);
  realDivide(&factor, const_MiToKm, &factor, &ctxtReal39);
  unitConversion(&factor, multiply, inverting);
}

void fnMgeuktok100M   (uint16_t multiplyDivide) {
  //note multiplyDivide is not used, as the formula is biderectional!
  //100*gallon_UK_equivalent  / (value), both ways
  //const_GalukToL / const_GalusToL * 33.7 * 100
  real_t factor;
  realMultiply(const_GaluseqE, const_100, &factor, &ctxtReal39);
  realMultiply(&factor, const_GalukToL, &factor, &ctxtReal39);
  realDivide(&factor, const_GalusToL, &factor, &ctxtReal39);
  unitConversion(&factor, multiply, inverting);
}


void fnCvtHMSHR   (uint16_t multiplyDivide) {
  if(multiplyDivide == divide) {
    fnHMStoTM(0);
    fnToReal(0);
  }
  else {
    fnHRtoTM(0);
    fnFrom_ms(0);
  }
}





/********************************************//**
 * \brief Converts power or field ratio to dB
 * dB = (10 or 20) * log10((power or field) ratio) this is the exact formula
 *
 * \param[in] unusedButMandatoryParameter uint16_t
 * \return void
 ***********************************************/
void fnCvtRatioDb(uint16_t tenOrTwenty) { // ten: power ratio   twenty: field ratio
  real_t reX;

  if(!getRegisterAsReal(REGISTER_X, &reX)) {
    return;
  }

  if(!saveLastX()) {
    return;
  }

  WP34S_Log10(&reX, &reX, &ctxtReal39);
  realMultiply(&reX, (tenOrTwenty == 10 ? const_10 : const_20), &reX, &ctxtReal39);

  convertRealToResultRegister(&reX, REGISTER_X, amNone);

  adjustResult(REGISTER_X, false, false, -1, -1, -1);
}



/********************************************//**
 * \brief Converts dB to power or field ratio
 * (power or field) ratio = 10^(dB / 10 or 20) this is the exact formula
 *
 * \param[in] unusedButMandatoryParameter uint16_t
 * \return void
 ***********************************************/
void fnCvtDbRatio(uint16_t tenOrTwenty) { // ten: power ratio   twenty: field ratio
  real_t reX;

  if(!getRegisterAsReal(REGISTER_X, &reX)) {
    return;
  }

  if(!saveLastX()) {
    return;
  }

  realDivide(&reX, (tenOrTwenty == 10 ? const_10 : const_20), &reX, &ctxtReal39);
  realPower10(&reX, &reX, &ctxtReal39);

  convertRealToResultRegister(&reX, REGISTER_X, amNone);

  adjustResult(REGISTER_X, false, false, -1, -1, -1);
}

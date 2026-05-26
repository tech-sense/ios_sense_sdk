#import <Foundation/Foundation.h>

// ── App Attest State ──────────────────────────────────────────────────────────
typedef NS_ENUM(NSInteger, JBAppAttestState) {
    JBAppAttestStateUnknown      = 0,
    JBAppAttestStatePassed       = 1,
    JBAppAttestStateFailed       = 2,
    JBAppAttestStateUnsupported  = 3,
};

// ── AppAttest (Bit 5 — Secure Enclave) ───────────────────────────────────────
@interface JBAppAttest : NSObject
+ (void)kickoff;
+ (JBAppAttestState)state;
@end

// ── KeychainGuard (Bit 6 — Persistence) ──────────────────────────────────────
@interface JBKeychainGuard : NSObject
+ (void)flagDeviceAsCompromised;
+ (BOOL)isDevicePreviouslyCompromised;
+ (void)clearFlag;
@end

// ── SignatureScanner (Bit 0 — Artifacts) ─────────────────────────────────────
@interface JBSignatureScanner : NSObject
+ (BOOL)hasJailbreakPaths;
+ (BOOL)hasJailbreakAppSchemes;
+ (BOOL)hasHiddenDirectories;
@end

// ── SystemProbe (Bits 1/2/3 — Stealth / Legacy / Kernel) ─────────────────────
@interface JBSystemProbe : NSObject
+ (int)stealthAccess:(const char *)path;
+ (BOOL)isSyscallPipelineTampered;
+ (BOOL)hasLegacyJailbreakIndicators;
+ (BOOL)canWriteOutsideSandbox;
+ (BOOL)hasJailbreakMounts;
+ (BOOL)sandboxChownTest;
+ (BOOL)canFork;
+ (BOOL)isDebuggerAttached;
+ (BOOL)inlineSyscallDetectsJailbreak;
+ (BOOL)isDeveloperModeEnabled;
+ (BOOL)isReducedSecurityKernel;
@end

// ── BehavioralDetector (Bit 4 — Memory Integrity) ────────────────────────────
@interface JBBehavioralDetector : NSObject
+ (BOOL)hasSuspiciousDylibs;
+ (BOOL)hasHiddenMachOImagesInMemory;
+ (BOOL)isTextSegmentWritable;
+ (BOOL)checkEntitlements;
@end

// ── MasterDetector (orchestrates all 7 bits) ─────────────────────────────────
@interface JBMasterDetector : NSObject
+ (uint32_t)performFullScan;
+ (BOOL)isJailbroken;
+ (NSDictionary<NSString *, id> *)detailedReport;
@end

# PhhIms on LineageOS 18.1 (Android 11) — W2017 port notes

This directory vendors the open-source SIP/IMS stack
[`krazey/ims`](https://github.com/krazey/ims) (PhhIms, GPL-2.0) and backports it
to the LineageOS 18.1 (Android 11) platform APIs so that the W2017 gets an
`android.telephony.ims.ImsService` at all.

The stock firmware only ships a 6.0.1 Samsung/QTI IMS stack
(`imscmservice` + `imsmanager.jar` + `com.sec.ims` inside 6.0.1
`telephony-common.jar`); none of it can run on Android 11, and the stock CTC
CSC does not even enable IMS. PhhIms replaces the whole AP-side IMS stack with
a userspace SIP/IMS client and only depends on the RIL for the IMS bearer.

## Backport changes vs. upstream

Language/toolchain (the AOSP 11 tree ships a `-Xbuild-file` patched Kotlin
**1.3.61**, upstream targets Kotlin 1.6+):

- dropped trailing commas (`6k+` occurrences), `lowercase()/uppercase()`,
  `Char.code`, `digitToInt()`, `minByOrNull`, `maxWithOrNull`,
  `toBooleanStrictOrNull`, `firstNotNullOfOrNull`, `buildList`,
  `replaceFirstChar`, `@OptIn`;
- replaced the two `kotlinx.coroutines` sites with daemon threads
  (`kotlinx_coroutines` is not in the 18.1 tree);
- the WebRTC AEC3 / rnnoise JNI libraries are not built (their source
  submodules are not vendored). They are only loaded when
  `persist.phh.ims.webrtc_aec_delay_ms > 0`, which defaults to 0.

Android 11 IMS API differences:

- `ImsService`: `createMmTelFeature(slotId)` / `createRcsFeature(slotId)` /
  `getConfig(slotId)` / `getRegistration(slotId)` instead of the
  `*ForSubscription` variants; the subscription id is resolved from
  `SubscriptionManager.getSubId(slotId)`.
- `ImsConfigImplBase` has no `setRcsClientConfiguration` /
  `triggerAutoConfiguration` in 11 — removed those overrides.
- `ServiceState` monitoring uses `PhoneStateListener` (11) instead of
  `TelephonyCallback` (12+); `NetworkRegistrationInfo.isRegistered()` instead
  of `isNetworkRegistered`.
- `ImsCallSessionListener.callSessionInitiatedFailed()` (11) instead of
  `callSessionInitiatingFailed()` (12+).
- `MmTelFeature.notifyIncomingCall(session, extras)` has no return value in 11;
  the framework sets the session listener itself.
- **`onFeatureReady()` is never called for the new ImsService API on Android
  11** (it is only wired through the legacy compat listener). The feature
  therefore bootstraps itself from `ImsService.createMmTelFeature()` and also
  starts the SIP handler when the framework-visible state becomes
  `STATE_READY`.
- `ImsFeature.initialize()` is final and never called for the new API in 11, so
  the protected `mContext` can be null; the feature falls back to the
  `ImsService` instance context.
- `AudioManager.clearCommunicationDevice()` is API 31+; called via reflection.
- `SmsManager.createForSubscriptionId()` does not exist in 11;
  `SmsManager.getSmsManagerForSubscriptionId()` is used instead.

Device-specific readiness fix:

- With 2G shut down (and this device having no 3G), `ServiceState.getState()`
  stays `OUT_OF_SERVICE` (voice) even though the PS domain is `HOME` on LTE.
  Upstream gates MMTEL `STATE_READY` on the overall state, which would never
  become true here. The readiness check now requires a **registered PS/LTE**
  registration instead (IMS only needs the PS bearer), and falls back to
  `ServiceState.getOperatorNumeric()` for the PLMN when the network
  registration info carries no registered PLMN.

## How it is wired into the build

| Piece | Location |
| --- | --- |
| app + static RRO | `device/samsung/veyronltectc/PhhIms/` (`PhhIms`, `PhhImsOverlay`) |
| product package | `device.mk` (`PRODUCT_PACKAGES += PhhIms`) |
| MMTEL binding | `overlay/packages/services/Telephony/res/values/config.xml` (`config_ims_mmtel_package=me.phh.ims`) |
| VoLTE platform flags | `overlay/frameworks/base/core/res/res/values/config.xml` (`config_device_volte_available`, `config_carrier_volte_available`) |
| privileged permissions | `common/configs/privapp-permissions-phhims.xml` |
| CSC IMS switch | `common/csc/others.xml` → `/system/csc/others.xml` |
| bring-up prop | `common/system.prop` (`persist.dbg.volte_avail_ovr=1`) |

## Verification so far (2026-11-25)

Working:

- `ImsResolver: device MMTEL package: me.phh.ims` and
  `Binding ImsService: ComponentInfo{me.phh.ims/me.phh.ims.PhhImsService}
  with features: [{s=0, f=MMTEL}]`.
- The app runs as uid 1000 and requests an IMS-capable network; the framework
  brings up the `ims` APN: `rmnet_data2` with an operator address, DNS and a
  default route on the IMS PDN.
- `PHH SipHandler`: realm `ims.mnc011.mcc460.3gppnetwork.org`, registration
  tech LTE, local address taken from the IMS link.

Blocked:

- **No P-CSCF.** The Samsung modem returns an empty P-CSCF set in the
  `SETUP_DATA_CALL` response (`RILD: [*] pcscf: 0.0.0.0`,
  `pcscfIp6Addr1: ::`, `RILJ: pcscf=[]`), for IPv6-only and IPv4-only IMS
  PDNs alike. The 3GPP DNS fallback cannot help: China Telecom does not
  publish NAPTR/SRV records for `ims.mnc011.mcc460.3gppnetwork.org`.
  P-CSCF on this platform is delivered in the PCO of the modem's **IMS
  profile** (`Modem config #2: index[2], iftype[4], ifname[imsd]`), which the
  RIL only uses from its own IMS PDN path (`DoIMSEpdnOperation`), not for an
  APN data call created by the framework (`DefinePdpContext: Profile not
  found: Create profile` → ad-hoc profile 3, no P-CSCF PCO).

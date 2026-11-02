# Copied motion, contact and release observations

`HIGGS_QueryObservationInterface` supplies a separate versioned C table without changing official001, the existing velocity service or the hand broker tables. The current table advertises motion, enabled-contact evidence, coherent release snapshots and controller tracking mappings. It exports no engine pointers or allocator ownership.

The service retains 256 motion records, 512 contacts, 64 release snapshots and 64 mappings. It reports actual retained history rather than an instantaneous complete overlap set. A copied query identifies the current broker epoch, physical hands, optional source/space, sequence cursor and minimum producer observation time. Records are returned in ascending sequence order. Capacity failure changes only the required count; other failures preserve outputs. Sequence values never wrap or repeat after epoch changes.

## Motion and provenance

Motion sources are the raw controller, resolved HIGGS palm and held rigid body. Spaces are Skyrim world axes or raw OpenVR tracking axes. Position is expressed in world Skyrim units or tracking metres. Linear velocity is metres/second; angular velocity is radians/second. Flags distinguish known pose, linear/angular velocity and a matching tracking mapping. Fields without their numeric flags are zero. A proper finite rotation is required for an available pose. Unavailable, invalid and disconnected controller records remain explicit observations with no fabricated zero pose.

Controller records distinguish the actual OpenVR device index, accepted tracking generation/capture ticket and broker context observed at capture entry. The controller records do not claim to be the same physical sample as a previously published palm or input snapshot. Both hands' raw/world records and the corresponding mapping are committed together or refused together. Duplicate, out-of-order or old-scope captures cannot overwrite current history. A missing HMD/world mapping can leave valid raw controller data while its world pose remains unavailable.

The mapping is derived from the public engine `HmdMatrixToNiTransform` operation and the observed world HMD transform. Converted basis probes establish axes, translation and world units per raw metre; the code does not substitute guessed conversion constants. A mapping record states `worldPosition = translation + rotation × rawTrackingPosition × unitsPerMeter`. World linear velocity uses the observed scale and the engine's game-unit/Havok-metre conversion; angular velocity rotates without a scale factor. The engine separately converts each pose's local orientation axes. Mapping IDs identify actual captures, not hardware recenter notifications.

The sampler checks that the tracking ticket remains accepted and that the broker epoch and physical-hand generations still match before publishing. Replaced worlds, load transitions and late observations are refused. Timestamps come from the producer's steady clock and identify callback observation time; they are not hardware acquisition timestamps. Callback silence retains historical samples. Consumers must select an age bound appropriate to their operation rather than treating a retained record as current tracking.

Palm poses come directly from the copied post-HIGGS snapshot. Held-body poses/velocities are read under the retained world's read lock, using retained wrapper/body references and verifying world membership. Body keys are observed Havok `m_uid` values scoped by the broker epoch. They are diagnostic observation keys, not saved identities, raw pointers or independently pinned lifetime handles.

## Contact evidence

The native contact hook records only contacts that remain enabled after HIGGS filtering. It distinguishes hand, weapon and held-object probes, physical hands, copied target reference/form type, observed body keys, world point, target-to-probe normal and both bodies' linear velocities. Used and potential flags preserve the engine event attributes. An enabled potential contact is not independently labelled a completed collision, impulse, damage commitment or permission to grab the target.

Contact records describe the event while its engine-owned bodies are valid. Retaining the copied record does not retain those bodies. Clients requiring an NPC attachment validate a current PLANCK endpoint; clients requiring anatomical interpretation obtain PPB coverage/contact data. HIGGS supplies hand-use permission separately. The history cannot prove that a target still exists, enumerate every current manifold or replace PLANCK hit outcomes.

For HIGGS-owned articulated finger parts, the separate [hand-parts table](articulated-hand-parts.md) copies the exact contact's part, set, owner, applied revision and native lifetime generation by this record's epoch and sequence. This association is captured by the producer, rather than reconstructed later from an observed Havok UID. Its bounded history can retain the original identity after body removal or UID reuse. The existing observation record layout remains unchanged.

## Release snapshots

`CopyReleaseObservation` looks up a retained HIGGS release event by its event ID in the current epoch. The producer captures the exact pair of hand records from that publication after both hands have been staged, including the released hand and the other hand. `hasMatchingSnapshot` explicitly distinguishes an event whose matching snapshot was available. Consumers must check it rather than constructing a release state from later polling. An evicted event returns not-found; a load changes the epoch and invalidates retained releases.

## Verification boundary

`python tests/run_observation_tests.py --build-dir <external-directory>` checks the actual store, public C layout/table, broker-produced coherent release pair, bounded queries, epoch handling, counter exhaustion and refusal/atomicity cases. It also executes the actual native controller-capture source using explicitly declared engine/VR conversion doubles, including transform/velocity scaling, missing/invalid devices and load/reentrant-conversion races. The complete native project is compiled separately against the compatible SKSE/Havok declarations. Neither those source checks nor successful linking establish hardware timing, actual converter semantics or game behavior; those require runtime qualification.

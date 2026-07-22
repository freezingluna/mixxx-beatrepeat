#include "effects/backends/builtin/beatrepeateffect.h"

#include <QtDebug>

#include "util/math.h"
#include "util/sample.h"

QString BeatRepeatEffect::getId() {
    return QStringLiteral("org.mixxx.effects.beatrepeat");
}

EffectManifestPointer BeatRepeatEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());

    pManifest->setAddDryToWet(false);
    pManifest->setEffectRampsFromDry(false);

    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Beat Repeat"));
    pManifest->setShortName(QObject::tr("Beat Repeat"));
    pManifest->setAuthor("The Mixxx Team");
    pManifest->setVersion("1.0");
    pManifest->setDescription(
            QObject::tr("Captures and repeats a segment of audio for a rhythmic "
                        "stutter or roll effect. Use with quantize to snap "
                        "repeat lengths to the beat grid."));

    EffectManifestParameterPointer duration = pManifest->addParameter();
    duration->setId("duration");
    duration->setName(QObject::tr("Duration"));
    duration->setShortName(QObject::tr("Duration"));
    duration->setDescription(QObject::tr(
            "Length of the repeat loop in beat fractions.\n"
            "1/1 - one beat\n"
            "1/2 - half beat\n"
            "1/4 - quarter beat\n"
            "1/8 - eighth beat\n"
            "1/16 - sixteenth beat\n"
            "1/32 - thirty-second beat"));
    duration->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    duration->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    duration->setRange(0.0, 0.25, 1.0);

    duration->appendStep(QPair<QString, double>(QObject::tr("1/32"), 0.0));
    duration->appendStep(QPair<QString, double>(QObject::tr("1/16"), 0.1));
    duration->appendStep(QPair<QString, double>(QObject::tr("1/8"), 0.2));
    duration->appendStep(QPair<QString, double>(QObject::tr("1/4"), 0.4));
    duration->appendStep(QPair<QString, double>(QObject::tr("1/2"), 0.6));
    duration->appendStep(QPair<QString, double>(QObject::tr("1/1"), 0.8));
    duration->appendStep(QPair<QString, double>(QObject::tr("2/1"), 1.0));

    EffectManifestParameterPointer quantize = pManifest->addParameter();
    quantize->setId("quantize");
    quantize->setName(QObject::tr("Quantize"));
    quantize->setShortName(QObject::tr("Quantize"));
    quantize->setDescription(QObject::tr(
            "Snap the repeat duration to the nearest beat fraction."));
    quantize->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    quantize->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    quantize->setRange(0, 1, 1);

    EffectManifestParameterPointer triplet = pManifest->addParameter();
    triplet->setId("triplet");
    triplet->setName(QObject::tr("Triplets"));
    triplet->setShortName(QObject::tr("Triplets"));
    triplet->setDescription(
            QObject::tr("When Quantize is enabled, divide the beat fractions "
                        "by 3 for triplet feel."));
    triplet->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    triplet->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    triplet->setRange(0, 0, 1);

    return pManifest;
}

void BeatRepeatEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pDurationParameter = parameters.value("duration");
    m_pQuantizeParameter = parameters.value("quantize");
    m_pTripletParameter = parameters.value("triplet");
}

void BeatRepeatEffect::processChannel(
        BeatRepeatGroupState* pGroupState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(enableState);

    double period = m_pDurationParameter->value();

    double delay_seconds;
    double min_delay;
    if (groupFeatures.beat_length.has_value()) {
        if (m_pQuantizeParameter->toBool()) {
            period = roundToFraction(period, 8);
            if (m_pTripletParameter->toBool()) {
                period /= 3.0;
            }
        }
        period = std::max(period, 1 / 32.0);
        delay_seconds = period * groupFeatures.beat_length->seconds;
        min_delay = 1 / 32.0 * groupFeatures.beat_length->seconds;
    } else {
        delay_seconds = std::max(period, 1 / 32.0);
        min_delay = 1 / 32.0;
    }

    if (delay_seconds < min_delay) {
        SampleUtil::copy(
                pOutput,
                pInput,
                engineParameters.samplesPerBuffer());
        return;
    }

    int delay_samples = static_cast<int>(delay_seconds *
            engineParameters.channelCount() * engineParameters.sampleRate());
    pGroupState->sample_count += engineParameters.samplesPerBuffer();
    if (pGroupState->sample_count >= delay_samples) {
        SampleUtil::copy(pGroupState->repeat_buf.data(),
                pInput,
                engineParameters.samplesPerBuffer());
        pGroupState->sample_count = 0;
    }

    SampleUtil::copy(
            pOutput,
            pGroupState->repeat_buf.data(),
            engineParameters.samplesPerBuffer());
}

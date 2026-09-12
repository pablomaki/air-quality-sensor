/* SKETCH — src/matter/matter_reporting.cpp
 * The ONLY place that writes the Matter data model, and the only place that knows
 * cluster units. Runs the writes on the CHIP thread via ScheduleWork.
 */
#include <aqs/state.h>
#include <aqs/events.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <app-common/zap-generated/ids/Clusters.h>

using namespace chip::app::Clusters;

namespace {

/* One row per quantity: where it goes and how it is scaled. Adding a quantity is
 * one row here plus one row in the ZAP/endpoint map -- nothing else. */
struct Binding {
	aqs_quantity  q;
	chip::EndpointId ep;
	chip::ClusterId  cluster;
	/* writer takes the float in our canonical unit and does the cluster's own
	 * scaling; nullopt when the value is not usable. */
	void (*write)(chip::EndpointId, const aqs_value &);
};

void WriteTemp(chip::EndpointId ep, const aqs_value &v)
{
	if (v.status == AQS_VALID || v.status == AQS_STALE) {
		TemperatureMeasurement::Attributes::MeasuredValue::Set(
			ep, static_cast<int16_t>(v.value * 100.0f));   /* 0.01 degC */
	} else {
		TemperatureMeasurement::Attributes::MeasuredValue::SetNull(ep);
	}
}

void WriteCo2(chip::EndpointId ep, const aqs_value &v)
{
	if (v.status == AQS_VALID || v.status == AQS_STALE) {
		CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Set(
			ep, v.value);                                  /* float ppm */
	} else {
		CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::SetNull(ep);
	}
}
/* ... WriteHumidity, WritePressure, WritePmX, WriteTvoc, WriteAirQuality ... */

constexpr Binding kBindings[] = {
	{ AQS_Q_TEMPERATURE, kEpTemperature, TemperatureMeasurement::Id, WriteTemp },
	{ AQS_Q_CO2,         kEpAirQuality,  CarbonDioxideConcentrationMeasurement::Id, WriteCo2 },
	/* ... */
};

/* Runs on the CHIP thread: stack lock is held for us, attribute writes are legal. */
void WriteAllOnChipThread(intptr_t arg)
{
	auto *s = reinterpret_cast<aqs_state *>(arg);
	for (const auto &b : kBindings) {
		if (!aqs_matter_endpoint_enabled(b.ep)) {
			continue;   /* hardware absent on this variant */
		}
		b.write(b.ep, s->q[b.q]);
	}
	AirQuality::Attributes::AirQuality::Set(
		kEpAirQuality, static_cast<AirQuality::AirQualityEnum>(s->air_quality));
	delete s;
}

} /* namespace */

/* Called from the telemetry thread on each report tick (or on-change trigger).
 * Never LockChipStack() from here: ScheduleWork avoids inversion against the radio. */
extern "C" void aqs_matter_publish(const struct aqs_state *snapshot)
{
	auto *copy = new aqs_state(*snapshot);
	chip::DeviceLayer::PlatformMgr().ScheduleWork(
		WriteAllOnChipThread, reinterpret_cast<intptr_t>(copy));
}

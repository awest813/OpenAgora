#include "SimulationContext.hxx"

void SimulationContext::reset()
{
  m_data = SimulationContextData{};
}

void SimulationContext::advanceMonth()
{
  ++m_data.month;
}

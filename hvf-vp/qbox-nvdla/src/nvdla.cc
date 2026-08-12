#include <systemc>

#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <NV_nvdla.h>

class Nvdla : public scsim::cmod::NV_nvdla
{
    sc_core::sc_signal<bool> m_irq;

    void forward_irq() { irq->write(m_irq.read()); }

public:
    SC_HAS_PROCESS(Nvdla);

    InitiatorSignalSocket<bool> irq;

    explicit Nvdla(const sc_core::sc_module_name& name): scsim::cmod::NV_nvdla(name), m_irq("irq_signal"), irq("irq")
    {
        nvdla_intr.bind(m_irq);
        SC_METHOD(forward_irq);
        sensitive << m_irq;
    }
};

extern "C" void module_register() { GSC_MODULE_REGISTER_C(Nvdla); }

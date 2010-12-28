
#include "types.h"

void Tp::registerTypes()
{
}

Tp::Feature::Feature(const QString &className, uint id, bool critical )
{
    Q_UNUSED( className );
    Q_UNUSED( id );
    Q_UNUSED( critical );
}

Tp::Feature::Feature(const Feature &other)
        :QPair<QString,uint>()
{
    Q_UNUSED( other );
}

Tp::Feature::~Feature()
{
}


//Q_DECLARE_METATYPE(Tp::SimplePresence)

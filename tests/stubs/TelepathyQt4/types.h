#ifndef _TelepathyQt4_types_h_HEADER_GUARD_
#define _TelepathyQt4_types_h_HEADER_GUARD_

#include "PendingOperation"
#include "Constants"

#include <QAtomicInt>
#include <QPair>
#include <QString>
#include <QSharedDataPointer>
#include <QSet>
#include <QMetaType>
#include <QDBusArgument>
#include <QtDBus>
#include <QDBusMetaType>

namespace Tp
{
    void registerTypes();

    struct Avatar
    {
        QByteArray avatarData;
        QString MIMEType;
    };


    template <class T>
    class SharedPtr;
    class RefCounted;

    class WeakData
    {
        Q_DISABLE_COPY(WeakData)

        public:
        WeakData(RefCounted *d) : d(d), weakref(0) { }

        RefCounted *d;
        mutable QAtomicInt weakref;
    };

    class RefCounted
    {
        Q_DISABLE_COPY(RefCounted)

    public:
        inline RefCounted() : strongref(0), wd(0) { }
        inline virtual ~RefCounted() { if (wd) { wd->d = 0; } }

        inline void ref() { strongref.ref(); }
        inline bool deref() { return strongref.deref(); }

        mutable QAtomicInt strongref;
        WeakData *wd;
    };

    template <class T>
    class WeakPtr
    {
    public:
        inline WeakPtr() : wd(0) { }
        inline WeakPtr(const WeakPtr<T> &o) : wd(o.wd) { if (wd) { wd->weakref.ref(); } }
        inline WeakPtr(const SharedPtr<T> &o)
        {
            if (o.d) {
                if (!o.d->wd) {
                    o.d->wd = new WeakData(o.d);
                }
                wd = o.d->wd;
                wd->weakref.ref();
            } else {
                wd = 0;
            }
        }
        inline ~WeakPtr()
        {
            if (wd && !wd->weakref.deref()) {
                if (wd->d) {
                    wd->d->wd = 0;
                }
                delete wd;
            }
        }

        inline bool isNull() const { return !wd || !wd->d || wd->d->strongref == 0; }
        inline operator bool() const { return !isNull(); }
        inline bool operator!() const { return isNull(); }

        inline WeakPtr<T> &operator=(const WeakPtr<T> &o)
        {
            WeakPtr<T>(o).swap(*this);
            return *this;
        }

        inline WeakPtr<T> &operator=(const SharedPtr<T> &o)
        {
            WeakPtr<T>(o).swap(*this);
            return *this;
        }

        inline void swap(WeakPtr<T> &o)
        {
            WeakData *tmp = wd;
            wd = o.wd;
            o.wd = tmp;
        }

        SharedPtr<T> toStrongRef() const { return SharedPtr<T>(*this); }

    private:
        friend class SharedPtr<T>;

        WeakData *wd;
    };


    class Object : public QObject, public RefCounted
    {
        Q_OBJECT
        Q_DISABLE_COPY(Object)

    Q_SIGNALS:
        void propertyChanged(const QString &propertyName);

    protected:
        Object(){}

        //void notify(const char *propertyName);

    private:
    };

    template <class T>
    class SharedPtr
    {
    public:
        inline SharedPtr() : d(0) { }
        explicit inline SharedPtr(T *d) : d(d) { if (d) { d->ref(); } }
        inline SharedPtr(const SharedPtr<T> &o) : d(o.d) { if (d) { d->ref(); } }
        explicit inline SharedPtr(const WeakPtr<T> &o)
        {
            if (o.wd && o.wd->d) {
                d = static_cast<T*>(o.wd->d);
                d->ref();
            } else {
                d = 0;
            }
        }
        inline ~SharedPtr()
        {
             if (d && !d->deref()) {
                delete d;
             }
        }

        inline void reset()
        {
            SharedPtr<T>().swap(*this);
        }

        inline T *data() const { return d; }
        inline const T *constData() const { return d; }
        inline T *operator->() { return d; }
        inline T *operator->() const { return d; }

        inline bool isNull() const { return !d; }
        inline operator bool() const { return !isNull(); }
        inline bool operator!() const { return isNull(); }

        inline bool operator==(const SharedPtr<T> &o) const { return d == o.d; }
        inline bool operator!=(const SharedPtr<T> &o) const { return d != o.d; }
        inline bool operator==(const T *ptr) const { return d == ptr; }
        inline bool operator!=(const T *ptr) const { return d != ptr; }

        inline SharedPtr<T> &operator=(const SharedPtr<T> &o)
        {
            SharedPtr<T>(o).swap(*this);
            return *this;
        }

        inline void swap(SharedPtr<T> &o)
        {
            T *tmp = d;
            d = o.d;
            o.d = tmp;
        }

        template <class X>
        static inline SharedPtr<T> staticCast(const SharedPtr<X> &src)
        {
            return SharedPtr<T>(static_cast<T*>(src.data()));
        }

        template <class X>
        static inline SharedPtr<T> dynamicCast(const SharedPtr<X> &src)
        {
            return SharedPtr<T>(dynamic_cast<T*>(src.data()));
        }

        template <class X>
        static inline SharedPtr<T> constCast(const SharedPtr<X> &src)
        {
            return SharedPtr<T>(const_cast<T*>(src.data()));
        }

        template <class X>
        static inline SharedPtr<T> qObjectCast(const SharedPtr<X> &src)
        {
            return SharedPtr<T>(qobject_cast<T*>(src.data()));
        }

    private:
        friend class WeakPtr<T>;

        T *d;
    };

    class Feature : public QPair<QString, uint>
    {
    public:
        explicit Feature(const QString &className ="", uint id = 0, bool critical = false);
        Feature(const Feature &other);
        ~Feature();

        Feature &operator=(const Feature &other);

        bool isCritical() const;

//    private:
//        struct Private;
//        friend struct Private;
//        QSharedDataPointer<Private> mPriv;
    };
    class Features : public QSet<Feature>
    {
    public:
        Features() { }
        Features(const Feature &feature) { insert(feature); }
        Features(const QSet<Feature> &s) : QSet<Feature>(s) { }
    };

    inline Features operator|(const Feature &feature1, const Feature &feature2)
    {
        return Features() << feature1 << feature2;
    };

    struct SimplePresence
    {
        uint type;
        QString status;
        QString statusMessage;
    };

    namespace MethodInvocationContextTypes
    {
        struct Nil
        {
        };
    }

    template<typename T1 = MethodInvocationContextTypes::Nil, typename T2 = MethodInvocationContextTypes::Nil,
             typename T3 = MethodInvocationContextTypes::Nil, typename T4 = MethodInvocationContextTypes::Nil,
             typename T5 = MethodInvocationContextTypes::Nil, typename T6 = MethodInvocationContextTypes::Nil,
             typename T7 = MethodInvocationContextTypes::Nil, typename T8 = MethodInvocationContextTypes::Nil>
    class MethodInvocationContext : public RefCounted
    {
        public:
        MethodInvocationContext():m_isFinished(false), m_isError(false){}
        bool isFinished() const { return m_isFinished; }
        bool isError() const { return m_isError; }
        QString errorName() const { return ""; }
        QString errorMessage() const { return ""; }

        void setFinished(const T1 &t1 = T1(), const T2 &t2 = T2(), const T3 &t3 = T3(),
                         const T4 &t4 = T4(), const T5 &t5 = T5(), const T6 &t6 = T6(),
                         const T7 &t7 = T7(), const T8 &t8 = T8())
        {
            Q_UNUSED(t1)
            Q_UNUSED(t2)
            Q_UNUSED(t3)
            Q_UNUSED(t4)
            Q_UNUSED(t5)
            Q_UNUSED(t6)
            Q_UNUSED(t7)
            Q_UNUSED(t8)
            ut_setIsFinished(true);
        }

        void setFinishedWithError(const QString &,const QString &)
        {
            ut_setIsFinished(true);
            ut_setIsError(true);
        }

    public: // ut
        void ut_setIsFinished(bool finished) {m_isFinished = finished;}
        void ut_setIsError(bool error) {m_isError = error;}

    private:
        bool m_isFinished;
        bool m_isError;
    };

    template<typename T1 = MethodInvocationContextTypes::Nil, typename T2 = MethodInvocationContextTypes::Nil,
             typename T3 = MethodInvocationContextTypes::Nil, typename T4 = MethodInvocationContextTypes::Nil,
             typename T5 = MethodInvocationContextTypes::Nil, typename T6 = MethodInvocationContextTypes::Nil,
             typename T7 = MethodInvocationContextTypes::Nil, typename T8 = MethodInvocationContextTypes::Nil>
    class MethodInvocationContextPtr : public SharedPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> >
    {
    public:
        inline MethodInvocationContextPtr() { }
        explicit inline MethodInvocationContextPtr(MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> *d)
            : SharedPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> >(d) { }
        inline MethodInvocationContextPtr(const SharedPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> > &o)
            : SharedPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> >(o) { }
        explicit inline MethodInvocationContextPtr(const WeakPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> > &o
    )
            : SharedPtr<MethodInvocationContext<T1, T2, T3, T4, T5, T6, T7, T8> >(o) { }
    };


    struct PropertyValue
    {
        uint identifier;
        QDBusVariant value;
    };

    QDBusArgument& operator<<(QDBusArgument& arg, const PropertyValue&);
    const QDBusArgument& operator>>(const QDBusArgument& arg, PropertyValue&);

    typedef QList<PropertyValue> PropertyValueList;

    struct HandleOwnerMap : public QMap<uint, uint>
    {
        inline HandleOwnerMap() : QMap<uint, uint>() {}
        inline HandleOwnerMap(const QMap<uint, uint>& a) : QMap<uint, uint>(a) {}

        inline HandleOwnerMap& operator=(const QMap<uint, uint>& a)
        {
            *(static_cast<QMap<uint, uint>*>(this)) = a;
            return *this;
        }
    };

    struct UIntList : public QList<uint>
    {
        inline UIntList() : QList<uint>() {}
        inline UIntList(const QList<uint>& a) : QList<uint>(a) {}

        inline UIntList& operator=(const QList<uint>& a)
        {
            *(static_cast<QList<uint>*>(this)) = a;
            return *this;
        }
    };

    struct MessagePart : public QMap<QString, QDBusVariant>
    {
        inline MessagePart() : QMap<QString, QDBusVariant>() {}
        inline MessagePart(const QMap<QString, QDBusVariant>& a) : QMap<QString, QDBusVariant>(a) {}

        inline MessagePart& operator=(const QMap<QString, QDBusVariant>& a)
        {
            *(static_cast<QMap<QString, QDBusVariant>*>(this)) = a;
            return *this;
        }
    };

    typedef QList<MessagePart> MessagePartList;

    struct HandleIdentifierMap : public QMap<uint, QString>
    {
        inline HandleIdentifierMap() : QMap<uint, QString>() {}
        inline HandleIdentifierMap(const QMap<uint, QString>& a) : QMap<uint, QString>(a) {}

        inline HandleIdentifierMap& operator=(const QMap<uint, QString>& a)
        {
            *(static_cast<QMap<uint, QString>*>(this)) = a;
            return *this;
        }
    };

    struct PropertySpec
    {
        uint propertyID;
        QString name;
        QString signature;
        uint flags;
    };

    bool operator==(const PropertySpec& v1, const PropertySpec& v2);
    inline bool operator!=(const PropertySpec& v1, const PropertySpec& v2)
    {
        return !operator==(v1, v2);
    }

    QDBusArgument& operator<<(QDBusArgument& arg, const PropertySpec&);
    const QDBusArgument& operator>>(const QDBusArgument& arg, PropertySpec&);


    typedef QList<PropertySpec> PropertySpecList;

    struct ServicePoint
    {
        uint servicePointType;
        QString service;
    };
    bool operator==(const ServicePoint& v1, const ServicePoint& v2);

    inline bool operator!=(const ServicePoint& v1, const ServicePoint& v2)
    {
        return !operator==(v1, v2);
    }
    QDBusArgument& operator<<(QDBusArgument& arg, const ServicePoint& val);
    const QDBusArgument& operator>>(const QDBusArgument& arg, ServicePoint& val);

    class DBusProxy;

    class Account;
    class AccountSet;
    class AccountManager;
    class Channel;
    class Connection;
    class Contact;
    class ContactManager;
    class TextChannel;
    class StreamedMediaChannel;
    class StreamedMediaStream;


    typedef SharedPtr<Account> AccountPtr;
    typedef SharedPtr<AccountSet> AccountSetPtr;
    typedef SharedPtr<AccountManager> AccountManagerPtr;
    typedef SharedPtr<Connection> ConnectionPtr;
    typedef SharedPtr<Channel> ChannelPtr;
    typedef QSharedPointer<Contact> ContactPtr;
    typedef SharedPtr<ContactManager> ContactManagerPtr;
    typedef SharedPtr<TextChannel> TextChannelPtr;
    typedef SharedPtr<StreamedMediaChannel> StreamedMediaChannelPtr;
    typedef SharedPtr<StreamedMediaStream> StreamedMediaStreamPtr;
    typedef SharedPtr<DBusProxy> DBusProxyPtr;
};

Q_DECLARE_METATYPE(Tp::SimplePresence);
Q_DECLARE_METATYPE(Tp::PropertySpec);
Q_DECLARE_METATYPE(Tp::PropertySpecList);
Q_DECLARE_METATYPE(Tp::PropertyValue);
Q_DECLARE_METATYPE(Tp::PropertyValueList);
Q_DECLARE_METATYPE(Tp::UIntList);
Q_DECLARE_METATYPE(Tp::ServicePoint);
Q_DECLARE_METATYPE(Tp::MessagePart);
Q_DECLARE_METATYPE(Tp::MessagePartList);
Q_DECLARE_METATYPE(Tp::Feature);
Q_DECLARE_METATYPE(Tp::Features);

#endif

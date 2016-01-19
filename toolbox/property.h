#ifndef PROPERTY_H
#define PROPERTY_H

/* Preprocessor macros PROPERTY_RW, PROPERTY_rW, PROPERTY_Rw, PROPERTY_rw
 * Create property named name_ of type type with read and write accessors.
 *
 * If the last 'r' is uppercase a standard definition is provided for the
 * read accessor name(), else only a declaration.
 *
 * If the last 'w' is uppercase a standard definition is provided for the
 * write accessor set_name(), else only a declaration.
 */
#define PROPERTY_RW( type, name, readaccess, writeaccess, memberaccess ) \
    memberaccess:       \
        type name##_;   \
    writeaccess:        \
        bool set_##name( type const& val ) { this->name##_ = val; return true; } \
    readaccess:         \
        type const& name() const { return this->name##_;}

#define PROPERTY_Rw( type, name, readaccess, writeaccess, memberaccess ) \
    memberaccess:       \
        type name##_;   \
    writeaccess:                            \
        bool set_##name( type const& val ); \
    readaccess:                                     \
        type const& name() const { return this->name##_;}

#define PROPERTY_rW( type, name, readaccess, writeaccess, memberaccess ) \
    memberaccess:       \
        type name##_;   \
    writeaccess:        \
        bool set_##name( type const& val ) { this->name##_ = val; return true; } \
    readaccess:                             \
        type const& name() const;

#define PROPERTY_rw( type, name, readaccess, writeaccess, memberaccess ) \
    memberaccess:       \
        type name##_;   \
    writeaccess:        \
        bool set_##name( type const& val ); \
    readaccess:                             \
        type const& name() const;

/* Preprocessor macra PROPERTY_RO
 * Create a property with only read access
 */
#define PROPERTY_RO( type, name ) \
    protected:          \
        type name##_;   \
    public:             \
        type const& name() const { return this->name##_;}

#endif // PROPERTY_H

#ifndef STRUCTPROPS_H
#define STRUCTPROPS_H

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

*******************************************************************************************/

#include <ossie/CorbaUtils.h>

struct equalizer_struct {
	equalizer_struct ()
	{
		low = 0;
		med = 0;
		hi = 0;
	};

    std::string getId() {
        return std::string("equalizer");
    };
	
	double low;
	double med;
	double hi;
};

inline bool operator>>= (const CORBA::Any& a, equalizer_struct& s) {
	CF::Properties* temp;
	if (!(a >>= temp)) return false;
	CF::Properties& props = *temp;
	for (unsigned int idx = 0; idx < props.length(); idx++) {
		if (!strcmp("low", props[idx].id)) {
			if (!(props[idx].value >>= s.low)) return false;
		}
		if (!strcmp("med", props[idx].id)) {
			if (!(props[idx].value >>= s.med)) return false;
		}
		if (!strcmp("hi", props[idx].id)) {
			if (!(props[idx].value >>= s.hi)) return false;
		}
	}
	return true;
};

inline void operator<<= (CORBA::Any& a, const equalizer_struct& s) {
	CF::Properties props;
	props.length(3);
	props[0].id = CORBA::string_dup("low");
	props[0].value <<= s.low;
	props[1].id = CORBA::string_dup("med");
	props[1].value <<= s.med;
	props[2].id = CORBA::string_dup("hi");
	props[2].value <<= s.hi;
	a <<= props;
};

inline bool operator== (const equalizer_struct& s1, const equalizer_struct& s2) {
    if (s1.low!=s2.low)
        return false;
    if (s1.med!=s2.med)
        return false;
    if (s1.hi!=s2.hi)
        return false;
    return true;
};

inline bool operator!= (const equalizer_struct& s1, const equalizer_struct& s2) {
    return !(s1==s2);
};

template<> inline short StructProperty<equalizer_struct>::compare (const CORBA::Any& a) {
    if (super::isNil_) {
        if (a.type()->kind() == (CORBA::tk_null)) {
            return 0;
        }
        return 1;
    }

    equalizer_struct tmp;
    if (fromAny(a, tmp)) {
        if (tmp != this->value_) {
            return 1;
        }

        return 0;
    } else {
        return 1;
    }
}


#endif

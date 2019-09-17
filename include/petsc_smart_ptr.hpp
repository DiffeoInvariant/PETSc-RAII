#ifndef PETSC_SMART_PTR_HPP
#define PETSC_SMART_PTR_HPP

#include <cstddef> //NULL
extern "C" {
#include <mpi.h>
#include <petscerror.h>
#include <petscsys.h>
#include <petscmat.h>
#include <petsc/src/sys/objects/inherit.h>
#include <petsc/private/matimpl.h>//struct _p_Mat
#include <stdlib.h>
}
#include <memory>
#include <string>
#include <utility>
#include <type_traits>



//TODO: decide if communicator is an integral part of the class (e.g. a member variable) or is a visitor

//base class
template<typename T>
class petsc_smart_ptr_base
{
public:

	//make everything noexcept 'cuz PETSc objects don't play well w/ C++ exceptions (yet, at least!)
	//ugly malloc call cuz we're doing manual memory management here. This default base ctor should really
	//probably never be called though, so it doesn't matter so much anyway
	petsc_smart_ptr_base() noexcept :
								m_ptr(malloc(sizeof(T))),
								m_ierr(0) 
	{
		PetscFunctionBeginHot;
		//check that malloc worked (m_ptr is not null)
		if(not m_ptr)
		{
			m_ierr = PETSC_ERR_MEM;
		}
		
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	};

	petsc_smart_ptr_base(T& ptr) noexcept : m_ptr(std::addressof(ptr))
	{
		PetscFunctionBeginHot;
		//TODO: should I do this?
		//increment the object's ref count
		m_ierr = PetscObjectReference((PetscObject)(m_ptr))
		//TODO: maybe some conditions to see if things look okay? (not sure exactly what that means yet)
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	};

	petsc_smart_ptr_base(T* ptr) noexcept : m_ptr(ptr)
	{
		PetscFunctionBeginHot;
		//TODO: same as above
		m_ierr = PetscObjectReference((PetscObject)(m_ptr))
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	};

		//copy ctor -- ptr is not const cuz we increment its pointer's refcount
	petsc_smart_ptr_base(petsc_smart_ptr_base& ptr) noexcept : m_ptr(ptr.m_ptr), m_ierr(ptr.m_ierr)
	{
		PetscFunctionBeginHot;
		//TODO: same as above
		m_ierr = PetscObjectReference((PetscObject)(ptr));
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	};	

	//move ctor
	petsc_smart_ptr_base(petsc_smart_ptr_base&& ptr) noexcept : m_ptr(ptr.m_ptr), m_ierr(ptr.m_ierr)
	{
		PetscFunctionBeginHot;
		//destroy the other pointer
		ptr.~petsc_smart_ptr_base();
		PetscFunctionReturnVoid;
	}

		

	//replace with `typedef T type` if we want to make compatible with old C++
	using type = T;
	
	//overload to send m_ierr to whichever process requests it
	void request_ierr(PetscError& ierr) const noexcept
	{
		PetscFunctionBegin;
		ierr = m_ierr;
		PetscFunctionReturnVoid();
	}

	PetscError request_ierr() const noexcept
	{
		PetscFunctionBegin;
		PetscFunctionReturn(m_ierr);
	}


	//returns raw pointer to the data
	T* get() noexcept
	{
		PetscFunctionBeginHot;
		PetscFunctionReturn(m_ptr);
	
	}

	//sets the given raw pointer equal to m_ptr
	PetscError get(T* ptr) noexcept
	{
		PetscFunctionBeginHot;
		ptr = m_ptr;
		PetscFunctionReturn(0);
	}


	//gets the reference count of the object
	PetscInt refcount() noexcept
	{
		PetscFunctionBegin;
		PetscInt refcnt;

		PetscObjectGetReference((PetscObject)(m_ptr), &refcnt);

		PetscFunctionReturn(refcnt);
	}

	PetscError refcount(PetscInt* cnt) noexcept
	{
		PetscFunctionBegin;
		PetscObjectReference((PetscObject)(m_ptr), cnt);
		PetscFunctionReturn(0);
	}


	/* this means that if you have some PETSc object wrapped in a petsc_smart_ptr<SomePetscType>,
	 * such as
	 *
	 * petsc_smart_ptr<SomePetscType> obj([some constructor info]);
	 *
	 * and want to access some member of obj, then doing
	 *
	 * auto thing = obj->attribute;
	 *
	 * is _the exact same thing_ as declaring a 
	 *
	 * SomePetscType*  CStyleObj;
	 *
	 * doing all the memory allocation that the petsc_smart_ptr<SomePetscType> constructor does,
	 * and then doing
	 *
	 * auto thing = CStyleObj->attribute;
	 */
	T* operator->() noexcept
	{
		PetscFunctionBeginHot;
		if(m_ptr == NULL)
		{
			//NOTE: maybe add a CHKERRQ here? seems more like something the user should have control over though,
			// as much as one has any control once one dereferences a null pointer...
			m_ierr = PETSC_ERR_POINTER;
		}

		PetscFunctionReturn(m_ptr);
	}

	const T* operator->() const noexcept
	{
		PetscFunctionBeginHot;
		if(m_ptr == NULL)
		{
			m_ierr = PETSC_ERR_POINTER;
		}

		PetscFunctionReturn(m_ptr);
	}
			

	T& operator*() noexcept
	{
		PetscFunctionBeginHot;
		PetscFunctionReturn(*m_ptr);
	}

	const T& operator() const noexcept
	{
		PetscFunctionBeginHot;
		PetscFunctionReturn(*m_ptr);
	}

	virtual ~petsc_smart_ptr_base() noexcept
	{
		PetscFunctionBegin;

		if(m_ptr) //if m_ptr is not already nulled
		{
		//anybody else have a problem and trying to tell us? if so, we can't delete. This shouldn't 
		//affect performance meaningfully, since generally, destructors for PETSc objects will be called at
		//the end of a (segment of a) computation, when new data is relatively more likely to be loaded anyway
		PetscInt flag;
		MPI_Status stat;//in case flag is true
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_SELF, &flag, &stat);
		
		if(flag){
			//TODO: implement this (the case where there is some incoming message)
			m_ierr = PETSC_ERR_SIG;
		}
		//if there's an error, crash before deallocating anything
		CHKERRQ(m_ierr);
		m_ierr = PetscObjectDereference((PetscObject)(m_ptr));
		//check that the object isn't being referenced by anyone else
		m_ierr = refcount(&flag);
		if(not m_ierr)//if the refcount is zero
		{
			PetscFree(m_ptr);
			//if that didn't work...
			if(m_ptr){
				m_ierr = PETSC_ERR_WRONGSTATE;
			}
		} 
		else
		{
			//some error, for now, just continue without freeing object, re-increment the refcount
			m_ierr = PetscObjectReference((PetscObject)(m_ptr));	
		}
		//make sure we don't need to crash (again)
		CHKERRQ(m_ierr);//TODO: maybe use CHKERRABORT(comm, m_ierr) instead?

		PetscFunctionReturnVoid();
		}
	}

	


protected:

	//error code for operations on the object
	PetscError m_ierr;

	T*		 m_ptr;//the object

};//class petsc_smart_ptr_base


//base template (type deduction fails, it's a type we haven't implemented yet)
template<typename T>
class petsc_smart_ptr : private petsc_smart_ptr_base<T>
{

public:

	constexpr petsc_smart_ptr() noexcept : petsc_smart_ptr_base() 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};

	petsc_smart_ptr(const T& ptr) noexcept : petsc_smart_ptr_base(ptr) 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};

	petsc_smart_ptr(const T* ptr) noexcept : petsc_smart_ptr_base(ptr) 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};
	

	~petsc_smart_ptr() noexcept
	{
		PetscFunctionBeginHot;
		//nothing to do here, base dtor takes care of it
		PetscFunctionReturnVoid();
	}
};



//matrix type specialization
using _p_Mat = struct _p_Mat;//'cuz C has typedef structs for some stupid reason
template<>
class petsc_smart_ptr<_p_Mat> : petsc_smart_ptr_base<_p_Mat>
{
	//takes pointer to matrix struct
	petsc_smart_ptr(Mat ptr, MPI_Comm comm=PETSC_COMM_WORLD) : petsc_smart_ptr_base(ptr)
	{
		PetscFunctionBeginHot;
		MatCreate(comm, &m_ptr);
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};

	petsc_smart_ptr(Mat ptr, MPI_Comm comm=PETSC_COMM_WORLD, const std::string& mat_t) : petsc_smart_ptr_base(ptr)
	{
		PetscFunctionBeginHot;
		MatCreate(comm, &m_ptr)
		m_ierr = set_type(mat_t);
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};
		

	void set_type(const std::string& mat_t)
	{
		PetscFunctionBegin;
		MatSetType(m_ptr, mat_t);
		PetscFunctionReturnVoid();
	}

	
	~petsc_smart_ptr()
	{
		//m_ptr is a _p_Mat*, so no overloaded operator&()
		MatDestroy(&m_ptr);
		//base dtor does the rest
	}
};



//petsc typedefs _p_Mat* to mat (typdef struct _p_Mat* Mat), and this class should hold a pointer to
// _p_Mat, not a pointer-to-pointer-to _p_Mat
using petsc_smart_ptr<Mat> = petsc_smart_ptr<_p_Mat>;

#endif //PETSC_SMART_PTR_HPP

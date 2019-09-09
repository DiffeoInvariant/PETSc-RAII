#ifndef PETSC_SMART_PTR_HPP
#define PETSC_SMART_PTR_HPP

#include <petsc/src/sys/objects/inherit.h>
#include <mpi.h>
#include <cstddef> //NULL
#include <petscerror.h>
#include <petscmat.h>
#include <stdlib.h>
#include <memory>
#include <new>


//base class
template<typename T>
class petsc_smart_ptr_base
{
public:

	//make everything noexcept 'cuz PETSc objects don't play well w/ C++ exceptions (yet, at least!)
	//ugly malloc call cuz we're doing manual memory management here. This default base ctor should really
	//probably never be called though, so it doesn't matter so much anyway
	constexpr petsc_smart_ptr_base() noexcept 
									: m_ptr(malloc(sizeof(T))), m_ierr(0) 
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

	constexpr petsc_smart_ptr_base(const T& ptr) noexcept 
												: m_ptr(std::addressof(ptr))
	{
		PetscFunctionBeginHot;
		//TODO: maybe some conditions to see if things look okay? (not sure exactly what that means yet)
		m_ierr = 0;
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	};

	constexpr petsc_smart_ptr_base(const T* ptr) noexcept
												: m_ptr(ptr)
	{
		PetscFunctionBeginHot;
		//TODO: same as above
		m_ierr = 0;
		CHKERRQ(ierr);

		PetscFunctionReturnVoid();
	};

	virtual ~petsc_smart_ptr_base()
	{
		PetscFunctionBegin;
		//anybody else have a problem and trying to tell us? if so, we can't delete. This shouldn't 
		//affect performance meaningfully, since generally, destructors for PETSc objects will be called at
		//the end of a (segment of a) computation, when new data is relatively more likely to be loaded anyway
		int flag;
		MPI_Status stat;//in case flag is true
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_SELF, &flag, &stat);
		
		if(flag){
			//TODO: implement this (the case where there is some incoming message
			m_ierr = PETSC_ERR_SIG;
		}
		//if there's an error, crash before deallocating anything
		CHKERRQ(m_ierr);

		PetscFree(m_ptr);
		//if that didn't work...
		if(m_ptr){
			m_ierr = PETSC_ERR_WRONGSTATE;
		}
		//make sure we don't need to crash (again)
		CHKERRQ(m_ierr);

		PetscFunctionReturnVoid();
	}
		
		

	//replace with `typedef T type` if we want to make compatible with old C++
	using type = T;
	
	//overload to send m_ierr to whichever process requests it
	constexpr void request_ierr(PetscError& ierr) const noexcept
	{
		PetscFunctionBegin;
		ierr = m_ierr;
		PetscFunctionReturnVoid();
	}

	constexpr PetscError request_ierr() const noexcept
	{
		PetscFunctionBegin;
		PetscFunctionReturn(m_ierr);
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

	constexpr petsc_smart_ptr() : petsc_smart_ptr_base() 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};

	constexpr petsc_smart_ptr(const T& ptr) : petsc_smart_ptr_base(ptr) 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};

	constexpr petsc_smart_ptr(const T* ptr) : petsc_smart_ptr(ptr) 
	{
		PetscFunctionBeginHot;
		m_ierr = PETSC_ERR_SUP;
		CHKERRQ(m_ierr);
		PetscFunctionReturnVoid();
	};
	

	~petsc_smart_ptr()
	{
		PetscFunctionBeginHot;
		//nothing to do here, base dtor takes care of it
		PetscFunctionReturnVoid();
	}
};

#endif //PETSC_SMART_PTR_HPP

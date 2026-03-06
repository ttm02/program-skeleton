program allocate_example
    implicit none

    integer, allocatable :: a(:)
    integer :: n, i, ios
    character(len = 32) :: arg

    n = 1337

    ! Allocate memory
    allocate(a(n))

    ! Initialize array
    do i = 1, n
        a(i) = i * 2
    end do

    print *, "Allocated array with size:", n
    do i = 1, n
        print *, "a(", i, ") = ", a(i)
    end do

    ! Free memory
    deallocate(a)

end program allocate_example

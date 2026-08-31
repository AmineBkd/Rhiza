#include "Input.h"

namespace Rhiza
{

void Input::beginFrame()
{
    mPrevious = mCurrent;
}

void Input::handleKeyEvent( Key key, bool isDown )
{
    if( key == Key::Unknown )
        return;

    mCurrent.set( static_cast<size_t>( key ), isDown );
}

void Input::releaseAll()
{
    mCurrent.reset();
}

bool Input::isKeyDown( Key key ) const
{
    return mCurrent.test( static_cast<size_t>( key ) );
}

bool Input::wasKeyPressed( Key key ) const
{
    const size_t index = static_cast<size_t>( key );
    return mCurrent.test( index ) && !mPrevious.test( index );
}

bool Input::wasKeyReleased( Key key ) const
{
    const size_t index = static_cast<size_t>( key );
    return !mCurrent.test( index ) && mPrevious.test( index );
}

}  // namespace Rhiza

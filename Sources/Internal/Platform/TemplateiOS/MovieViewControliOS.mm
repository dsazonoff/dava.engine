#if !defined(__DAVAENGINE_COREV2__)

#include "MovieViewControliOS.h"

#import <MediaPlayer/MediaPlayer.h>
#import <Platform/TemplateiOS/HelperAppDelegate.h>

namespace DAVA
{
//Use unqualified UIWebView and UIScreen from global namespace, i.e. from UIKit
using ::UIScreen;

MovieViewControl::MovieViewControl()
{
    moviePlayerController = [[MPMoviePlayerController alloc] init];

    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;
    [player setShouldAutoplay:FALSE];
    [player.view setUserInteractionEnabled:NO];

    if ([player respondsToSelector:@selector(loadState)])
    {
        [player setControlStyle:MPMovieControlStyleNone];
        player.scalingMode = MPMovieScalingModeFill;
    }

    HelperAppDelegate* appDelegate = [[UIApplication sharedApplication] delegate];
    [[appDelegate renderViewController].backgroundView addSubview:player.view];
}

MovieViewControl::~MovieViewControl()
{
    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;

    [player.view removeFromSuperview];
    [player release];
    moviePlayerController = nil;
}

void MovieViewControl::Initialize(const Rect& rect)
{
    SetRect(rect);
}

void MovieViewControl::OpenMovie(const FilePath& moviePath, const OpenMovieParams& params)
{
    NSURL* movieURL = [NSURL fileURLWithPath:[NSString stringWithCString:moviePath.GetAbsolutePathname().c_str() encoding:NSASCIIStringEncoding]];

    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;
    [player setScalingMode:(MPMovieScalingMode)ConvertScalingModeToPlatform(params.scalingMode)];
    [player setContentURL:movieURL];
}

void MovieViewControl::SetRect(const Rect& rect)
{
    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;

    CGRect playerViewRect = player.view.frame;

    Rect physicalRect = VirtualCoordinatesSystem::Instance()->ConvertVirtualToPhysical(rect);
    playerViewRect.origin.x = physicalRect.x + VirtualCoordinatesSystem::Instance()->GetPhysicalDrawOffset().x;
    playerViewRect.origin.y = physicalRect.y + VirtualCoordinatesSystem::Instance()->GetPhysicalDrawOffset().y;
    playerViewRect.size.width = physicalRect.dx;
    playerViewRect.size.height = physicalRect.dy;

    // Apply the Retina scale divider, if any.
    DAVA::float32 scaleDivider = Core::Instance()->GetScreenScaleFactor();
    playerViewRect.origin.x /= scaleDivider;
    playerViewRect.origin.y /= scaleDivider;
    playerViewRect.size.height /= scaleDivider;
    playerViewRect.size.width /= scaleDivider;

    // Use decltype as CGRect::CGSize::width/height can be float or double depending on architecture 32-bit or 64-bit
    playerViewRect.size.width = std::max<decltype(playerViewRect.size.width)>(0.0, playerViewRect.size.width);
    playerViewRect.size.height = std::max<decltype(playerViewRect.size.width)>(0.0, playerViewRect.size.height);

    [player.view setFrame:playerViewRect];
}

void MovieViewControl::SetVisible(bool isVisible)
{
    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;
    [player.view setHidden:!isVisible];
}

void MovieViewControl::Play()
{
    [(MPMoviePlayerController*)moviePlayerController play];
}

void MovieViewControl::Stop()
{
    [(MPMoviePlayerController*)moviePlayerController stop];
}

void MovieViewControl::Pause()
{
    [(MPMoviePlayerController*)moviePlayerController pause];
}

void MovieViewControl::Resume()
{
    [(MPMoviePlayerController*)moviePlayerController play];
}

bool MovieViewControl::IsPlaying() const
{
    MPMoviePlayerController* player = (MPMoviePlayerController*)moviePlayerController;
    return (player.playbackState == MPMoviePlaybackStatePlaying);
}

int MovieViewControl::ConvertScalingModeToPlatform(eMovieScalingMode scalingMode)
{
    switch (scalingMode)
    {
    case scalingModeFill:
    {
        return MPMovieScalingModeFill;
    }

    case scalingModeAspectFill:
    {
        return MPMovieScalingModeAspectFill;
    }

    case scalingModeAspectFit:
    {
        return MPMovieScalingModeAspectFit;
    }

    case scalingModeNone:
    default:
    {
        return MPMovieScalingModeNone;
    }
    }
}
}

#endif // !__DAVAENGINE_COREV2__

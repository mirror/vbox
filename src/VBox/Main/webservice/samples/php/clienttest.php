<?php

/*
 * Sample client for the VirtualBox webservice, written in PHP.
 *
 * Run the VirtualBox web service server first; see the VirtualBOx
 * SDK reference for details.
 *
 * Copyright (C) 2009 Sun Microsystems, Inc.
 * Contributed by James Lucas (mjlucas at eng.uts.edu.au).
 *
 * The following license applies to this file only:
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

require_once('./vboxServiceWrappers.php');

//Connect to webservice
$connection = new SoapClient("vboxwebService.wsdl", array('location' => "http://localhost:18083/"));

//Logon to webservice
$websessionManager = new IWebsessionManager($connection);
// Enable if authentication is used.
//$virtualbox = $websessionManager->logon("username","password");

//Get a list of registered machines
$machines = $virtualbox->machines;

//Take a screenshot of the first vm we find that is running
foreach ($machines as $machine)
{
    if ( 'Running' == $machine->state )
    {
        $session = $websessionManager->getSessionObject($virtualbox->handle);
        $uuid = $machine->id;
        $virtualbox->openExistingSession($session, $uuid);
        try
        {
            $console = $session->console;
            $display = $console->display;
            $screenWidth = $display->width;
            $screenHeight = $display->height;
            $imageraw = $display->takeScreenShotSlow($screenWidth, $screenHeight);
            $session->close();
            $filename = './screenshot.png';
            echo "Saving screenshot of " . $machine->name . " (${screenWidth}x${screenHeight}) to $filename\n";
            $image = imagecreatetruecolor($screenWidth, $screenHeight);

            for ($height = 0; $height < $screenHeight; $height++)
            {
                for ($width = 0; $width < $screenWidth; $width++)
                {
                    $start = ($height*$screenWidth + $width)*4;
                    $red = $imageraw[$start];
                    $green = $imageraw[$start+1];
                    $blue = $imageraw[$start+2];
                    //$alpha = $imageraw[$start+3];

                    $colour = imagecolorallocate($image, $red, $green, $blue);

                    imagesetpixel($image, $width, $height, $colour);
                }
            }

            imagepng($image, $filename);
        }
        catch (Exception $ex)
        {
            // Ensure we close the VM Session if we hit a error, ensure we don't have a aborted VM
            echo $ex->getMessage();
            $session->close();
        }
        break;
    }
}

$websessionManager->logoff($virtualbox->handle);

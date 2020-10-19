#!/usr/bin/perl
########################################################################
#  Usage:
#
#    build.pl 
#
#  Arguments:
#
#    [help] | [[bi=<bif>|clean|check|compile|purify|optimize|debug|profiling|all|ignore|killall]]
#    Look subroutine showHelp for further details
#
########################################################################
#  Description:
#
#    This is the build script for the ida libraries and modules.
#
########################################################################
#
#   File:      deliver2qa.pl
#   Revision:  1.0
#   Date:      09-JAN-2009 13:10:34
#
########################################################################
#
################ main program ############################
#

use XML::Simple; # qw(:strict);
use Data::Dumper;

my $referencebasepath="/__notinitialized";

my $platformpath="/__notinitialized";
my $buildId="/__notinitialized";
my $deliverypath="/__notinitialized";       # path where delivery files are collected;
my $deliverybasepath="/__notinitialized";   # path (without os) where delivery files are collected;
my $operatingsystem="/__notinitialized";
my $osname="/__notinitialized";
my $err;
my $referencebasepath="/qadelivery/irfa/ida/03.00";

#
# begin of main script
#


print "\n";
print "***** runtime level delivery\n";
print "\n";



getprojectnameandreferencepath ();
selectsourcedirectory ();   # get operating system 
getbuildId ();         # read the number of the rtl to be delivered
checktargetdirectory ();    # check if targetdirectory (in qadelivery) is empty 

getdeliverybasepath ();     # get the temporary directory where the delivery files are to be collected
preparedeliverydirectory ();# prepare the dir where the delivery files are collected

downloadfiles ();           # copy files to temporary delivery directory
checksourcecontent ();      # check if the files exist and are ok ...
makereleaseinfo ();
deleteMetaDataFiles ();
adjustaccessrights ();      # yes in the temporary delivery directory

copytoreference ();         # i.e. to /qadelivery/...


print "\n";
print "***** runtime level delivery finished\n";
print"\n";

sub selectsourcedirectory
{
    my $answer;
    my $found = 0;
    #
    # select operating system 
    #
    
    print "INPUT: Operating system :\n";
    print " 1 win\n 2 aix5.2\n 4 hpux11.11\n 7 SLES10\n ";

    while ( ! $found )
      {
        print ">>>>>: ";
        $answer=<STDIN>;
        chomp($answer);

        if ( $answer eq "win" || $answer eq "1" )
          { # 1
            $platformpath="Windows2000";
            $osname="Windows 2000/XP";
            $operatingsystem="windows";
            $found = 1;
          }
        elsif ( $answer eq "aix5.2" || $answer eq "2"  )
          { # 2
            $platformpath="AIX52";
            $osname="AIX 5.2";
            $operatingsystem="aix5.2";
            $found = 1;
            $shlibext=".a";
            $shlinklibext="25$shlibext";
          }
        elsif ( $anser eq "hpux11.11"  || $answer eq "4" )
          { # 4
            $platformpath="HP-UX11";
            $osname="HP-UX 11.11";
            $operatingsystem="hpux11.11";
            $found = 1;
            $shlibext=".sl";
            $shlinklibext="$shlibext.25";
          }
        elsif ( $answer eq "hpux11.0" )
          {
            $platformpath="HP-UX11";
            $osname="HP-UX 11.0";
            $operatingsystem="hpux11.0";
            $found = 1;
          }
        elsif ( $answer eq "SLES10" || $answer eq "7" )
          { # 7
            $platformpath="SuSE-Linux10";
            $osname="SLES 10";
            $operatingsystem="sles10";
            $linux="true";
            $found = 1;
            $shlibext=".so";
            $shlinklibext="$shlibext.27";
          }
      }
}



sub getprojectnameandreferencepath
{
}


sub getbuildId
{
  #
  # selection of the buildId should work automatically
  # read it from BuildInfo.xml
  #
  my $buildInfoFile="$platformpath/BuildInfo.xml";
  unless ( open (INPUT, "< $buildInfoFile") )
    {
      die "Can't read '$buildInfoFile'!";
    }

  my $xmlBuildInfo;
  read ( INPUT, $xmlBuildInfo, -s INPUT);

  $xmlBuildInfo =~ s/[\n\r]*//g; # get rid of eol characters

  my $buildInfo = XMLin($xmlBuildInfo, 'KeyAttr' => [ ]);

  $buildId =  $buildInfo->{Software}->{'bi:buildId'};
  my $buildNo;
  my $buildOs;
  ($buildNo,$buildOs) = split ( /\./, $buildId );
    print "Current OS: $platformpath and buildOs Number from buildId $buildId is $buildOs\n";

}



sub getdeliverybasepath
  {
    #
    # choose the location where the files to be delivered are collected
    #
    print ">>>>>: Path to the delivery directory where the files to be delivered are collected\n   default is " . $ENV{'HOME'} . "/delivery\n   note: the contents of the subdirectory $operatingsystem will be deleted!\n";
    print ">>>>>: ";
    $deliverybasepath=<STDIN>;
    chomp ( $deliverybasepath );

    if ( $deliverybasepath eq "" )
      {
		  $deliverybasepath=$ENV{'HOME'} . "/delivery";
        print "$HOME\n";
      }
    print "INFO : Using $deliverybasepath\n";
    #
    # create directory for the runtime-release
    #
    if ( ! (-d $deliverybasepath) )
      {
        mkdir("$deliverybasepath",0755);
        
        print "INFO : New directory $deliverybasepath created\n";
      }
  }


sub checktargetdirectory
{
  #    
  # check if there is an appropriate target directory in the /reference tree
  #
  $referencepath = "$referencebasepath/$buildId";
  
  if ( (-e "$referencepath/BuildInfo.xml") )
    {
      print "INFO : runtime level $buildId probably already delivered\n";
      exit 1;
    }
  else
    {
      mkdir("$referencebasepath/$buildId",0755);
      print "INFO : New directory $referencebasepath/$buildId created\n";
    }
}

sub preparedeliverydirectory
{
    #
    # free subdirectory for a certain delivery
    #
    $deliverypath="$deliverybasepath/$operatingsystem";
    if ( (-d "$deliverypath"))
      {
		  print "INFO : directory $deliverypath will be removed now\n";
        system ("rm -rf $deliverypath");
      }


    print "INFO : create directory $deliverypath\n";
    mkdir("$deliverypath",0755);

    #
    # construct missing directories
    #
    if ( $operatingsystem ne "windows" )
      {
        my @newdirs=("custom", "config");
        foreach $i (@newdirs)
          {
            print "INFO : mkdir  $deliverypath/$i\n";
            mkdir ("$deliverypath/$i",0755);
          }
      }
}

sub downloadfiles
{
    my $msipath;
    my $found;

    system ("cp  $platformpath/BuildInfo.xml $deliverypath/BuildInfo.xml");

    if ( $operatingsystem eq "windows" )
      {
        print "INFO : collect files for MSI package delivery for $buildId\n";

	
        print "INFO : cp $platformpath/ida.03.00.msi $deliverypath/ida.03.00.$buildId.msi\n";
        system ("cp $platformpath/ida.03.00.msi $deliverypath/ida.03.00.$buildId.msi");
   
      }
    else
      {
        downloadfilesforserver ();
      }

}


sub downloadfilesforserver
{
  #
  # download of the directories from needed for runtime-release
  #

  my @loaddirs=("bin", "templates", "dat");
  foreach  $d (@loaddirs)
    {
      print "INFO : cp -r $d $deliverypath\n";
      system ("cp -r $d $deliverypath");
    }
    
  print "INFO : cp -r $platformpath/modules $deliverypath\n";
  system ("cp -r $platformpath/modules $deliverypath");
    
  system ("cp  $platformpath/BuildInfo.xml $deliverypath/BuildInfo.xml");
}    

sub checkExistence 
  {
     
     my $file=shift;     
	
     if (!(-e "$file")) 
       {
	  print "$file does not exist!\n";
	  $err++;
	  return 1;
       }
     
     return 0;
  }

sub checkExistenceAndSize
  {
     my $file=shift;
     
     $return=checkExistence("$file");

     if ($return != 0) 
       {
	  return 1;
       }
     
     if (!(-s "$file")) 
       {
	  print "$file is empty!";
	  $err++;
       }
  }

sub checkExistenceXBitAndSize
  {
     my $file=shift;
     
     $return=checkExistence("$file");

     if ($return != 0) 
       {
	  return 1;
       }
     
     if (!(-s "$file")) 
       {
	  print "$file is empty!\n";
	  $err++;
	  return;
       }

     if (!(-x "$file")) 
       {
	  print "$file is empty!\n";
	  $err++;
	  return;
       }
     
  }

sub checksourcecontent 
{
  #
  # check existence and size of all binaries in the bin or modules folder
  #
  my $i;
  $err = 0;

  if ( $operatingsystem eq "windows" )
    {
      print ("checking $deliverypath/ida.03.00.$buildId.msi\n");
      checkExistenceAndSize ("$deliverypath/ida.03.00.$buildId.msi");
    }
  else
    {
      my @modules = ("IdaTdfProcess");
    
      foreach $i (@modules)
        {
          checkExistenceXBitAndSize ("$deliverypath/modules/$i");
        }
    }
  
	
  if ( $err == 0 )
    {
      if ( $operatingsystem eq "windows" )
        {
          print "***** MSI package exists.\n";
        }
      else
        {
          print "***** All processes in ./modules exist.\n";
        }
    }
  else
    {
      print "***** Since some files are missing the script is stopped now!!!\n";
		
      exit 1;
    }
}


sub adjustaccessrights
  {
   print "INFO : adjust permissions\n";

   # set every file permission to 444, note that this overrides the exceutable x-bits
   print "INFO : adjust file permissions to 444\n";
   system ("find  $deliverypath -type f |xargs chmod 444"); 

   # set every directory permission to 755
   print "INFO : adjust directory permissions to 755\n";
   system ("find  $deliverypath -type d |xargs chmod 755");
   
   if ($operatingsystem ne "windows")
     {
      print "INFO : adjust executable permissions to 555\n";
      system ("chmod 555 $deliverypath/bin/*");
      system ("chmod 555 $deliverypath/modules/*");
   }
   else
     {
       print "INFO : adjust msi permissions to 555\n";
      system ("chmod 555 $deliverypath/ida.03.00.*");
   }
}


sub copytoreference
{
  #
  # copy ida delivery directory tree to the reference machine
  #

  print "copying $deliverypath/* to $referencepath\n";
  system ("cp -r $deliverypath/* $referencepath");

}

sub makereleaseinfo 
{
    chdir $deliverypath;
    print "generating releaseInfo\n";
    system ("/scriptpool/buildinfo/genreleaseinfo");
    if ( $operatingsystem eq "windows" )
      {
        system ("mv ReleaseInfo ReleaseInfo.txt");
      }
}


sub deleteMetaDataFiles
  {
    chdir $deliverypath;
    print "delete metadata if present\n";
    system ("find . -name '.metadata'|xargs /bin/rm -rf");
  }



$Id$

Directory descriptions:
    ./                  The Test Manager.
    ./batch/            Batch scripts to be run via cron.
    ./cgi/              CGI scripts (we'll use standard CGI at first).
    ./core/             The core Test Manager logic (model).
    ./htdocs/           Files to be served directly by the web server.
    ./htdocs/css/       Style sheets.
    ./htdocs/images/    Graphics.
    ./webui/            The Web User Interface (WUI) bits. (Not sure if we will
                        do model-view-controller stuff, though. Time will show.)

I.  Running a Test Manager instance with Docker:

  - This way should be preferred to get a local Test Manager instance running
    and is NOT meant for production use!

  - Install docker-ce and docker-compose on your Linux host (not tested on
    Windows yet). Your user must be able to run the Docker CLI (see Docker documentation).

  - Type "kmk" to get the containers built, "kmk start|stop" to start/stop them
    respectively. To start over, use "kmk clean". For having a peek into the container
    logs, use "kmk logs".

    To administrate / develop the database, an Adminer instance is running at
    http://localhost:8080

    To access the actual Test Manager instance, go to http://localhost:8080/testmanager/

  - There are two ways of doing development with this setup:

    a. The Test Manager source is stored inside a separate data volume called
       "docker_vbox-testmgr-web". The source will be checked out automatically on
       container initialization.  Development then can take part within that data
       container. The initialization script will automatically pull the sources
       from the public OSE tree, so make sure this is what you want!

    b. Edit the (hidden) .env file in this directory and change VBOX_TESTMGR_DATA
       to point to your checked out VBox root, e.g. VBOX_TESTMGR_DATA=/path/to/VBox/trunk


II. Steps for manually setting up a local Test Manager instance for development:

  - Install apache, postgresql, python, psycopg2 (python) and pylint.

  - Create the database by executing 'kmk load-testmanager-db' in
    the './db/' subdirectory.   The default psql parameters there
    requies pg_hba.conf to specify 'trust' instead of 'peer' as the
    authentication method for local connections.

  - Use ./db/partial-db-dump.py on the production system to extract a
    partial database dump (last 14 days).

  - Use ./db/partial-db-dump.py with the --load-dump-into-database
    parameter on the development box to load the dump.

  - Configure apache using the ./apache-template-2.4.conf (see top of
    file for details), for example:

        Define TestManagerRootDir "/mnt/scratch/vbox/svn/trunk/src/VBox/ValidationKit/testmanager"
        Define VBoxBuildOutputDir "/tmp"
        Include "${TestManagerRootDir}/apache-template-2.4.conf"

    Make sure to enable cgi (a2enmod cgi && systemctl restart apache2).

  - Default htpasswd file has users a user 'admin' with password 'admin' and a
    'test' user with password 'test'.  This isn't going to get you far if
    you've  loaded something from the production server as there is typically
    no 'admin' user in the 'Users' table there.  So, you will need to add your
    user and a throwaway password to 'misc/htpasswd-sample' using the htpasswd
    utility.

  - Try http://localhost/testmanager/ in a browser and see if it works.



N.B. For developing tests (../tests/), setting up a local test manager will be
     a complete waste of time.  Just run the test drivers locally.

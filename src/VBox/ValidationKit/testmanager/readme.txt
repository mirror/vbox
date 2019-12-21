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


Steps for setting up a local Test Manager instance for development:

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









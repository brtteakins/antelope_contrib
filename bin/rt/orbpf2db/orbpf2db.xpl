require "getopts.pl" ;
use Datascope;
use orb;
 
# Kent Lindquist
# Lindquist Consulting
# orbpf2db
# July, 2003

sub database_prep {
	my( $wantschema ) = pop( @_ );
	my( $dbname ) = pop( @_ );

	if( ! defined( @db ) ) { 

		if( -e "$dbname" ) {
			
			@db = dbopen( $dbname, "r+" );

			$haveschema = dbquery( @db, dbSCHEMA_NAME );

			if( $haveschema ne $wantschema ) {

				elog_complain( "Wrong schema in '$dbname' for current packet\n" );
				return 0;

			} else {

				return 1;
			}

		} else {
			
			dbcreate( $dbname, $wantschema );

			@db = dbopen( $dbname, "r+" );
		}
		
	} else {

		$haveschema = dbquery( @db, dbSCHEMA_NAME );

		if( $haveschema ne $wantschema ) {

			elog_complain( "Wrong schema in '$dbname' for current packet\n" );
			return 0;

		} else {

			return 1;
		}
	}
}

$Pf = "orbpf2db.pf";
$match = ".*/pf.*";
$write_mode = "overwrite";

if ( ! &Getopts('s:w:f:p:m:v') || @ARGV != 2 ) { 

    	die ( "Usage: orbpf2db [-p pffile] [-m match] [-f from] [-w mode] orb database\n" ) ; 

} else {
	
	$orbname = $ARGV[0];
	$dbname = $ARGV[1];
}

if( $opt_p ) {
	
	$Pf = $opt_p;
}

%trans = %{pfget( $Pf, "translations" )};

if( $opt_m ) {
	
	$match = $opt_m;
}

if( $opt_w ) {

	if( $opt_w =~ /^(overwrite|add)$/ ) {
		
		$write_mode = $opt_w;

	} else {
		
		die( "-w mode must be 'overwrite' or 'add'\n" );
	}
}

$orb = orbopen( $orbname, "r&" );

orbselect( $orb, $match );

if( $opt_f && ( ! $opt_s || ! -e "$opt_s" ) ) {
	
	orbposition( $orb, $opt_f );
	$pktid = orbtell( $orb );

} elsif( $opt_f ) {

	print STDERR "Ignoring -f in favor of existing state-file\n";
}

if( $opt_s ) {

	$stop = 0;
	exhume( $opt_s, \$stop, 15 );
	resurrect( "pktid", \$pktid );
	orbseek( $orb, "$pktid" );
}

for(;;) {
	($pktid, $srcname, $time, $packet, $nbytes) = orbreap( $orb );

	if( $opt_s ) {

		bury();
	}

	($result, $pkt) = unstuffPkt( $srcname, $time, $packet, $nbytes ); 

	if( $result ne "Pkt_pf" ) {
		if( $opt_v ) {
			printf "Received a $result, skipping\n";
		}
		next;
	}

	( $key ) = grep( "$srcname", keys( %trans ) );
	
	next if( ! defined( $key ) );

	$schema = $trans{$key}{"schema"};

	next unless database_prep( $dbname, $schema );

	%tables = %{$trans{$key}{"tables"}};

	foreach $table ( keys( %tables ) ) {

		@dbscratch = dblookup( @db, "", "$table", "", "dbSCRATCH" );
		@dbtable = dblookup( @db, "", "$table", "", "dbALL" );

		%fieldmap = %{$trans{$key}{"tables"}{$table}};

		if( defined( $fieldmap{FOREACH} ) ) {

			%arrays = %{pfget( $pkt->pf, $fieldmap{FOREACH} )};

			foreach $key ( keys( %arrays ) ) {

				foreach $field ( keys( %fieldmap ) ) {

					next if( $field eq "FOREACH" );

					$pattern = $fieldmap{$field};

					if( $pattern =~ /^FOREACH$/ ) {
						
						$value = $key;

					} elsif( $pattern =~ /^TIME:/ ) {

						$pattern =~ s/FOREACH/$key/;
						$pattern =~ s/^TIME://;
						$value = pfget( $pkt->pf, $pattern );
						$value =~ s/^\s*([\d-.]+).*/$1/;

					} else {

						$pattern =~ s/FOREACH/$key/;
						$value = pfget( $pkt->pf, $pattern );
					}

					dbputv( @dbscratch, "$field", $value );
				}

				if( $write_mode eq "add" ) {
	
					dbadd( @dbtable );

				} elsif( $write_mode eq "overwrite" ) {
	
					@records = dbmatches( @dbscratch, @dbtable, "hook_$table" );	
					@records = sort {$a <=> $b} @records;
					if( ! defined( @records ) || 
					    ( $recno = shift( @records ) ) !~ /^\d+$/ ) {
	
						$newrec = dbadd( @dbtable );
	
					} else {
	
						@dbreplace = @dbtable;
						$dbreplace[3] = $recno;
						dbput( @dbreplace );
					}
					#@dbmoo = dbsubset( @db, "serveraddress == \"132.239.4.66\" && serverport == 6510 && thread == 297861131" );
					#$myn = dbquery( @dbmoo, dbRECORD_COUNT );
					#print "Myn is $myn\n";
				}
			}

		} else {
			
			foreach $field ( keys( %fieldmap ) ) {

				$pattern = $fieldmap{$field};

				if( $pattern =~ /^TIME:/ ) {

					$pattern =~ s/^TIME://;
					$value = pfget( $pkt->pf, $pattern );
					$value =~ s/^\s*([\d-.]+).*/$1/;

				} else {
			
					$value = pfget( $pkt->pf, $pattern );
				}

				dbputv( @dbscratch, "$field", $value );
			}

			if( $write_mode eq "add" ) {
				
				dbadd( @dbtable );
			
			} elsif( $write_mode eq "overwrite" ) {

				@records = dbmatches( @dbscratch, @dbtable, "hook_$table" );

				if( ! defined( @records ) ) {
	
					dbadd( @dbtable );
	
				} else {

					@records = sort {$a <=> $b} @records;
					@dbreplace = @dbtable;
					$dbreplace[3] = shift( @records );
					dbput( @dbreplace );
				}
			}
		}
	}
}
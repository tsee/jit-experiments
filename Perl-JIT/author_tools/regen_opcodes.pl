#!perl
use 5.14.0;
use warnings;

# Define op definition's columns in text format
use constant {
  AST_CONST   => 0,
  AST_CLASS   => 1,
  AST_NAME    => 2,
  PERL_CONST  => 3,
  AST_FLAGS   => 4,
  OPTIONS     => 5,
};
my @cols = (AST_CONST, AST_CLASS, AST_NAME, PERL_CONST, AST_FLAGS, OPTIONS);

# Valid ops for generating filtering macro
my %valid_root_ops;
my %valid_ops;

# Read op defs from text format
open my $fh, "<", "author_tools/opcodes" or die $!;
my @op_defs;
while (<$fh>) {
  chomp;

  # special smart comment line...
  if (/^\s*#!\s*(\w+)\s*=(.*)$/) {
    my ($name, $value) = ($1, $2);
    if ($name eq 'jittable_ops') {
      $valid_ops{$_} = 1 for split /\s+/, $value;
    }
    elsif ($name eq 'jittable_root_ops') {
      $valid_root_ops{$_} = 1 for split /\s+/, $value;
    }
    else {
      die "Invalid smart comment: '$_'";
    }
  }

  s/#.*$//;
  next if !/\S/;

  my @cols = split /\s*,\s*/, $_;
  $cols[AST_FLAGS] = join '|', map {$_ ? "PJ_ASTf_$_" : $_} split /\|/, $cols[AST_FLAGS];
  push @op_defs, \@cols;
}
close $fh;
#use Data::Dumper; warn Dumper \@op_defs;

# Determine maximum column lengths for readable output
my @max_col_length;
for (@cols) {
  $max_col_length[$_] = max_col_length(\@op_defs, $_);
}

# Write data file for inclusion in pj_ast_terms.c
my $data_fh = prep_ops_data_file();

print $data_fh "static const char *pj_ast_op_names[] = {\n";
foreach my $op (@op_defs) {
  printf $data_fh "  %-*s // %-*s (%s)\n",
    $max_col_length[AST_NAME]+3, qq{"$op->[AST_NAME]",},
    $max_col_length[AST_CONST], $op->[AST_CONST], $op->[AST_CLASS];
}
print $data_fh "};\n\n";

print $data_fh "static unsigned int pj_ast_op_flags[] = {\n";
foreach my $op (@op_defs) {
  printf $data_fh "  %-*s // %-*s (%s)\n",
    $max_col_length[AST_FLAGS]+1, qq{$op->[AST_FLAGS],},
    $max_col_length[AST_CONST], $op->[AST_CONST], $op->[AST_CLASS];
}
print $data_fh "};\n\n#endif\n";
close $data_fh;

# Generate actual op constant enum list for inclusion in pj_ast_terms.h
my $enum_fh = prep_ops_enum_file();
print $enum_fh "typedef enum {\n";

my $cur_class;
foreach my $op (@op_defs) {
  if (not defined $cur_class
      or $cur_class ne $op->[AST_CLASS])
  {
    $cur_class = $op->[AST_CLASS];
    print $enum_fh "\n  // Op class: $cur_class\n";
  }

  printf $enum_fh "  %s,\n", $op->[AST_CONST];
}

my $first_and_last = find_first_last_of_class(\@op_defs);
#use Data::Dumper; warn Dumper $first_and_last;
print $enum_fh "\n  // Op class ranges\n";
foreach my $class_data (@$first_and_last) {
  my ($cl, $first, $last) = @$class_data;
  $cl =~ /^pj_opc_(.*)$/ or die;
  my $prefix = "pj_${1}_";
  print $enum_fh "\n";
  print $enum_fh "  ${prefix}FIRST = $first,\n";
  print $enum_fh "  ${prefix}LAST  = $last,\n";
}
print $enum_fh "\n  // Global op range:\n  pj_op_FIRST = " . $op_defs[0][AST_CONST] . ",\n";
print $enum_fh "  pj_op_LAST = " . $op_defs[-1][AST_CONST] . "\n";

print $enum_fh "} pj_op_type;\n\n#endif\n";
close $enum_fh;


# Now generate macro to select the root / non-root OP types to ASTify
my $op_switch_fh = prep_ast_gen_op_switch_file();
print $op_switch_fh "// Macros to determine which Perl OPs to ASTify\n";
foreach my $op (@op_defs) {
  if (not(grep $_ eq 'nonroot', split /\|/, $op->[OPTIONS])) {
    $valid_root_ops{ $op->[PERL_CONST] } = 1;
  }
  $valid_ops{ $op->[PERL_CONST] } = 1;
}
print $op_switch_fh "\n#define IS_AST_COMPATIBLE_ROOT_OP_TYPE(otype) ( \\\n     ";
print $op_switch_fh join " \\\n  || ", map "otype == $_", sort keys %valid_root_ops;
print $op_switch_fh ")\n\n";
print $op_switch_fh "\n#define IS_AST_COMPATIBLE_OP_TYPE(otype) ( \\\n     ";
print $op_switch_fh join " \\\n  || ", map "otype == $_", sort keys %valid_ops;
print $op_switch_fh ")\n\n";

print $op_switch_fh "\n\n#endif\n";
close $op_switch_fh;


sub generic_header {
  my $fh = shift;
  my $name = shift;
  print $fh <<HERE;
#ifndef $name
#define $name
// WARNING: Do not modify this file, it is generated!
// Modify the generating script $0 and its data file
// "author_tools/opcodes" instead!

HERE
}

sub prep_ops_data_file {
  my $file = "src/pj_ast_ops_data-gen.inc";
  open my $data_fh, ">", $file or die $!;
  generic_header($data_fh, "PJ_AST_OPS_DATA_GEN_INC_");
  return $data_fh;
}

sub prep_ops_enum_file {
  my $file = "src/pj_ast_ops_enum-gen.inc";
  open my $data_fh, ">", $file or die $!;
  generic_header($data_fh, "PJ_AST_OPS_ENUM_GEN_INC_");
  return $data_fh;
}

sub prep_ast_gen_op_switch_file {
  my $file = "src/pj_ast_op_switch-gen.inc";
  open my $data_fh, ">", $file or die $!;
  generic_header($data_fh, "PJ_AST_OP_SWITCH_GEN_INC_");
  return $data_fh;
}

sub max_col_length {
  my $ary = shift;
  my $index = shift;
  my $max = 0;
  for (@$ary) {
    $max = length($_->[$index]) if length($_->[$index]) > $max; 
  }
  return $max;
}

sub find_first_last_of_class {
  my $op_defs = shift;
  my @first_and_last;
  my $first_of_class;
  my $cur_class;
  my $prev;
  foreach my $op (@$op_defs) {
    if (not defined $cur_class) {
      $cur_class = $op->[AST_CLASS];
      $first_of_class = $op->[AST_CONST];
    }
    elsif ($op->[AST_CLASS] ne $cur_class) {
      push @first_and_last, [$cur_class, $first_of_class, $prev->[AST_CONST]];
      $cur_class = $op->[AST_CLASS];
      $first_of_class = $op->[AST_CONST];
    }
    $prev = $op;
  }
  push @first_and_last, [$cur_class, $first_of_class, $prev->[AST_CONST]];
  return \@first_and_last;
}

#!/usr/bin/env perl
use strict;
use warnings;

use Test::More;
use Math::Prime::Util::GMP qw/consecutive_integer_lcm/;

plan tests => 101 + 1;

my @lcms = qw/
1
1
2
6
12
60
60
420
840
2520
2520
27720
27720
360360
360360
360360
720720
12252240
12252240
232792560
232792560
232792560
232792560
5354228880
5354228880
26771144400
26771144400
80313433200
80313433200
2329089562800
2329089562800
72201776446800
144403552893600
144403552893600
144403552893600
144403552893600
144403552893600
5342931457063200
5342931457063200
5342931457063200
5342931457063200
219060189739591200
219060189739591200
9419588158802421600
9419588158802421600
9419588158802421600
9419588158802421600
442720643463713815200
442720643463713815200
3099044504245996706400
3099044504245996706400
3099044504245996706400
3099044504245996706400
164249358725037825439200
164249358725037825439200
164249358725037825439200
164249358725037825439200
164249358725037825439200
164249358725037825439200
9690712164777231700912800
9690712164777231700912800
591133442051411133755680800
591133442051411133755680800
591133442051411133755680800
1182266884102822267511361600
1182266884102822267511361600
1182266884102822267511361600
79211881234889091923261227200
79211881234889091923261227200
79211881234889091923261227200
79211881234889091923261227200
5624043567677125526551547131200
5624043567677125526551547131200
410555180440430163438262940577600
410555180440430163438262940577600
410555180440430163438262940577600
410555180440430163438262940577600
410555180440430163438262940577600
410555180440430163438262940577600
32433859254793982911622772305630400
32433859254793982911622772305630400
97301577764381948734868316916891200
97301577764381948734868316916891200
8076030954443701744994070304101969600
8076030954443701744994070304101969600
8076030954443701744994070304101969600
8076030954443701744994070304101969600
8076030954443701744994070304101969600
8076030954443701744994070304101969600
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
718766754945489455304472257065075294400
69720375229712477164533808935312303556800
69720375229712477164533808935312303556800
69720375229712477164533808935312303556800
69720375229712477164533808935312303556800
/;

foreach my $n (0..100) {
  is( consecutive_integer_lcm($n), $lcms[$n], "consecutive_integer_lcm($n)" );
}

is(
    consecutive_integer_lcm(2000),
    '151117794877444315307536308337572822173736308853579339903227904473000476322347234655122160866668946941993951014270933512030194957221371956828843521568082173786251242333157830450435623211664308500316844478617809101158220672108895053508829266120497031742749376045929890296052805527212315382805219353316270742572401962035464878235703759464796806075131056520079836955770415021318508272982103736658633390411347759000563271226062182345964184167346918225243856348794013355418404695826256911622054015423611375261945905974225257659010379414787547681984112941581325198396634685659217861208771400322507388161967513719166366839894214040787733471287845629833993885413462225294548785581641804620417256563685280586511301918399010451347815776570842790738545306707750937624267501103840324470083425714138183905657667736579430274197734179172691637931540695631396056193786415805463680000',
    "consecutive_integer_lcm(2000)"
  );

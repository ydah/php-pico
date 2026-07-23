--TEST--
string search and slice vector 012
--FILE--
<?php
$text = 'cneedle12m';
echo strpos($text, 'needle12'), ':', (str_contains($text, 'needle12') ? 1 : 0), ':', (str_starts_with($text, 'c') ? 1 : 0), ':', (str_ends_with($text, 'm') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle12

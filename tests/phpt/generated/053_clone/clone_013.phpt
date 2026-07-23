--TEST--
object clone and property COW vector 013
--FILE--
<?php
class GeneratedClone013 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone013([16, 31]);
$copy = clone $original;
$copy->items[0] = 125;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
16,31:125,31

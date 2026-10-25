#include "Types.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>

// Short names are convenient inside an implementation file. The shared header
// keeps std:: prefixes, so including it never changes somebody else's names.
using namespace std;

namespace veil {

const array<string, HeroCount>& heroNames() {
    static const array<string, HeroCount> names{"Rowan Vale", "Dr Iona Reed", "Silas Venn"};
    return names;
}

const array<string, 8>& chapterNames() {
    static const array<string, 8> names{"Rain Ward", "Brass Market", "Salt Docks", "Mercy Hospital",
        "Painted Mile", "Archive Below", "Bell Foundry", "Pale Conservatory"};
    return names;
}

const vector<EnemyDef>& enemies() {
    // The AI type changes actual combat decisions, rather than only changing
    // a creature's name. Stats then distinguish members of the same family.
    static const vector<EnemyDef> data{
        {0,"Hollow Porter","A hired carrier whose employer removed every reason to stop working.",Ai::Melee,30,7,1,1,3,8,false},
        {1,"Lamp Widow","A streetlight spirit that spits burning oil from beyond a blade's reach.",Ai::Ranged,25,7,0,4,2,7,false},
        {2,"Mourner's Hound","It follows the scent of a grief its master no longer remembers.",Ai::Stalker,28,8,0,1,5,12,false},
        {3,"Ledger Warden","A municipal bailiff wearing pages of unpaid debts as armor.",Ai::Guardian,43,7,4,1,2,6,false},
        {4,"Memory Leech","A discarded recollection that feeds by making a fresh wound familiar.",Ai::Leech,29,8,1,2,3,10,false},
        {5,"Tin Apostle","A wind-up evangelist carrying a sermon that summons smaller listeners.",Ai::Summoner,34,6,2,3,2,6,false},
        {6,"Glass Notary","Its shots certify that the victim was always supposed to be hurt.",Ai::Ranged,30,9,1,5,2,9,false},
        {7,"Smile Broker","A smiling stranger who trades places when the bargain turns sour.",Ai::Trickster,31,8,1,3,4,12,false},
        {8,"Dockside Bulwark","A diving suit filled with the weight of an entire drowned crew.",Ai::Brute,55,11,3,1,2,5,false},
        {9,"Tide Cantor","A shorebound singer that calls the names the sea has learned.",Ai::Summoner,35,8,1,4,2,7,false},
        {10,"Salt Wraith","A fast, brittle outline left behind when someone forgot how to swim.",Ai::Stalker,29,10,0,1,5,14,false},
        {11,"Harbor Lantern","A lighthouse lens that concentrates its warnings into artillery.",Ai::Artillery,33,11,1,6,1,5,false},
        {12,"Mercy Orderly","It still believes that any living patient is a failed treatment.",Ai::Melee,40,10,2,1,3,8,false},
        {13,"Dream Surgeon","A physician-shaped nightmare that repairs other nightmares first.",Ai::Medic,34,7,1,4,3,9,false},
        {14,"White Ward Giant","An abandoned restraint gown stretched around an impossible patient.",Ai::Brute,59,12,3,1,2,5,false},
        {15,"Candle Nurse","She draws warmth from the wounded to keep the ward's last candle lit.",Ai::Leech,35,10,1,3,3,10,false},
        {16,"Paper Acrobat","A folded playbill with a knife concealed in every crease.",Ai::Stalker,32,11,1,1,5,15,false},
        {17,"Mirror Understudy","An actor who steals the safest position in another person's scene.",Ai::Trickster,36,10,2,4,4,13,false},
        {18,"Velvet Usher","A theatre attendant determined that nobody leaves before the curtain.",Ai::Guardian,49,10,4,2,2,6,false},
        {19,"Laughing Marionette","It calls more empty puppets whenever its audience falls silent.",Ai::Summoner,39,9,1,4,3,9,false},
        {20,"Index Seraph","An archivist's stamp firing corrections across an entire reading room.",Ai::Artillery,38,12,2,6,2,7,false},
        {21,"Blank Inspector","A Bureau investigator whose own report contains no surviving details.",Ai::Ranged,42,11,2,5,3,11,false},
        {22,"Furnace Penitent","A foundry worker sealed inside a confession made of hot iron.",Ai::Brute,62,13,4,1,2,5,false},
        {23,"Forgotten Witness","A surviving fragment that heals its fellow witnesses before itself.",Ai::Medic,41,9,2,4,3,10,false},
        {24,"The Mourning Beadle","He guards an empty coffin and orders grief to stand in formation.",Ai::Guardian,96,12,5,2,2,7,true},
        {25,"The Grin Collector","A broker wearing the last happy expressions of his debtors.",Ai::Trickster,92,13,3,4,4,13,true},
        {26,"The Drowned Admiral","The captain of a fleet whose cargo is the city's unacknowledged dead.",Ai::Summoner,108,13,4,4,2,8,true},
        {27,"The Suture Saint","A celebrated healer who can no longer distinguish a cure from erasure.",Ai::Medic,112,14,3,4,3,9,true},
        {28,"Maestro of Borrowed Faces","His final performance turns every escape route into another stage.",Ai::Stalker,116,16,3,2,5,15,true},
        {29,"Registrar Null","The author of a city charter that edits people instead of paper.",Ai::Artillery,124,16,4,6,2,8,true},
        {30,"The Iron Cantor","A living tuning fork forged from the testimony of nine generations.",Ai::Brute,148,18,6,2,2,6,true},
        {31,"Vicar of the Hollow Hour","The first investigator to discover the bargain, and its last willing servant.",Ai::Leech,158,18,5,4,4,12,true}
    };
    return data;
}

const vector<ItemDef>& items() {
    static const vector<ItemDef> data = [] {
        vector<ItemDef> result;
        // A helper gives every item its array index as ID. Save files store
        // those small IDs instead of copying the entire description repeatedly.
        auto weapon = [&](string name, string description, WeaponClass type, int tier, Perk perk,
                          int attackBonus, int focusBonus = 0, int defenseBonus = 0) {
            result.push_back({static_cast<int>(result.size()),name,description,Slot::Weapon,type,
                attackBonus,defenseBonus,0,focusBonus,0,30 + tier * 95,tier * 2,perk});
        };
        // The three first weapons are the party's starting loadout, in order.
        weapon("Bureau Service Sabre","Rowan's issued blade. The edge is honest even when the paperwork is not.",WeaponClass::Blade,0,Perk::Bleed,4);
        weapon("Ashwood Physician's Cane","Iona's walking cane steadies the hand and renews a little strength.",WeaponClass::Cane,0,Perk::Regen,3,3);
        weapon("Threaded Fortune Deck","Silas marked these cards himself. Their advice is untrustworthy; their focus is not.",WeaponClass::Charm,0,Perk::Focus,3,4);
        weapon("Watchman's Six-Shooter","A practical revolver that rewards deliberate shots over panic.",WeaponClass::Revolver,0,Perk::None,5);
        weapon("Rainwatch Carbine","A long barrel for streets too dangerous to cross.",WeaponClass::Rifle,0,Perk::Focus,4,2);
        weapon("Brass Knuckle Ward","An engraved gauntlet that turns a clenched fist into a small barricade.",WeaponClass::Gauntlet,0,Perk::Guard,4,0,1);
        weapon("Widow's Promise","A blade that remembers the shape of a wound after it has closed.",WeaponClass::Blade,1,Perk::Bleed,8);
        weapon("Broker's Final Offer","Its black cylinder takes a little vitality with every settled account.",WeaponClass::Revolver,1,Perk::Drain,8);
        weapon("Tideglass Crook","Sea glass in the handle seems to refill between difficult decisions.",WeaponClass::Cane,1,Perk::Focus,7,6);
        weapon("Locket of the Unlost","An empty photograph protects its bearer without asking whose face is missing.",WeaponClass::Charm,1,Perk::Guard,6,7,1);
        weapon("Harbor Bell Rifle","A weighted firing mechanism sometimes leaves its target stunned by the report.",WeaponClass::Rifle,1,Perk::Stun,8,1);
        weapon("Orderly's Iron Hand","A restraint device repurposed into an instrument of determined survival.",WeaponClass::Gauntlet,1,Perk::Regen,7,0,3);
        weapon("Curtain-Cutter","Silas recognizes the maker. He refuses to discuss what the blade once cut.",WeaponClass::Blade,2,Perk::Stun,12,2);
        weapon("The Seventh Chamber","Six chambers are visible. The seventh appears when a debtor is wounded.",WeaponClass::Revolver,2,Perk::Bleed,12,2);
        weapon("Saint's Rebuttal","A surgeon's argument that keeping someone alive is still worth the trouble.",WeaponClass::Cane,2,Perk::Regen,10,9,2);
        weapon("Mask of the Last Applause","A charm that draws strength from the space between an audience's breaths.",WeaponClass::Charm,2,Perk::Drain,10,10);
        weapon("Index Longrifle","Its sight places a red correction mark over an enemy's best idea.",WeaponClass::Rifle,2,Perk::Focus,12,6);
        weapon("Foundry Oath","Each engraved promise makes these iron fingers a little harder to break.",WeaponClass::Gauntlet,2,Perk::Guard,11,2,4);
        weapon("Daybreak, Unwritten","A sabre forged without a name, so no contract can claim its bearer.",WeaponClass::Blade,3,Perk::Bleed,17,3,1);
        weapon("Mercy's Last Word","A revolver that answers the cruelest bargain with a stolen heartbeat.",WeaponClass::Revolver,3,Perk::Drain,16,4);
        weapon("Cane of the Returning Name","Every knot in its wood is a patient Iona refused to let disappear.",WeaponClass::Cane,3,Perk::Regen,13,13,3);
        weapon("The Honest Fool","A plain card that makes no prediction. That is the source of its power.",WeaponClass::Charm,3,Perk::Focus,13,16,2);
        weapon("Witness at Dawn","The rifle's long sight holds a place for someone who has not returned yet.",WeaponClass::Rifle,3,Perk::Stun,17,7);
        weapon("Hands That Remember","A foundry gauntlet lined with handwritten names instead of steel rivets.",WeaponClass::Gauntlet,3,Perk::Guard,16,5,6);

        const array<string,8> armorNames{"Rainproof Bureau Coat","Ledger-Leather Vest","Saltproof Greatcoat","Physician's Ward Robe",
            "Velvet Understudy Coat","Archivist's Chain Lining","Foundry Witness Plate","Mantle of the Unforgotten"};
        const array<string,8> charmNames{"Mira's Brass Button","Broker's Honest Scale","Unmailed Sea Letter","Candle of Small Mercies",
            "Ticket for an Empty Seat","Fragment of the First Charter","Untuned Bell Shard","A Name Kept Freely"};
        const array<string,8> bootNames{"Inspector's Street Boots","Market Runner's Soles","Harbor Wader Boots","Silent Ward Slippers",
            "Acrobat's Ribbon Boots","Archive Step-Shoes","Foundry Ashwalkers","Dawnwalker's Promise"};
        for (int chapter = 0; chapter < 8; ++chapter)
            result.push_back({static_cast<int>(result.size()),armorNames[chapter],
                "Armor from " + chapterNames()[chapter] + ": trades offense for dependable protection and a larger health reserve.",
                Slot::Armor,WeaponClass::Blade,0,2 + chapter,10 + chapter * 4,0,0,40 + chapter * 33,chapter,
                chapter >= 5 ? Perk::Guard : Perk::None});
        for (int chapter = 0; chapter < 8; ++chapter)
            result.push_back({static_cast<int>(result.size()),charmNames[chapter],
                "A personal token from " + chapterNames()[chapter] + ". Its significance creates focus rather than raw armor.",
                Slot::Charm,WeaponClass::Charm,chapter / 3,0,chapter * 2,3 + chapter * 2,0,38 + chapter * 31,chapter,
                chapter % 2 == 0 ? Perk::Focus : Perk::Regen});
        for (int chapter = 0; chapter < 8; ++chapter)
            result.push_back({static_cast<int>(result.size()),bootNames[chapter],
                "Footwear from " + chapterNames()[chapter] + ". Better positioning can save more lives than another point of damage.",
                Slot::Boots,WeaponClass::Blade,0,chapter / 3,chapter * 2,0,chapter >= 4 ? 2 : 1,35 + chapter * 28,chapter,
                chapter >= 6 ? Perk::Guard : Perk::None});
        return result;
    }();
    return data;
}

const vector<SkillDef>& skills() {
    // Power is a percentage used by combat; costs and cooldowns make skills
    // alternatives to basic attacks rather than an endlessly repeatable upgrade.
    static const vector<SkillDef> data{
        {0,"Measured Cut","A precise close strike that turns a small opening into real damage.",0,3,1,0,150,1,1,SkillKind::Strike},
        {1,"Ward the Living","Take a defensive stance and shield a nearby companion.",0,3,3,0,100,2,2,SkillKind::Guard},
        {2,"Fool's Footwork","Slip to a chosen free tile and escape a dangerous formation.",0,4,5,0,100,2,4,SkillKind::Blink},
        {3,"Break the Rhythm","A forceful interruption that stuns an enemy's next plan.",0,5,2,0,110,3,7,SkillKind::Stun},
        {4,"Reaping Arc","Sweep a small cluster with a broad sabre cut.",0,6,2,1,135,3,10,SkillKind::Strike},
        {5,"A Name Reclaimed","Strike with the conviction of everyone the Bureau tried to erase.",0,8,3,1,190,4,13,SkillKind::Drain},
        {6,"Field Dressing","Restore a companion's health before the next wound becomes fatal.",1,3,4,0,155,1,1,SkillKind::Heal},
        {7,"Clear the Mind","Restore focus so a companion can use their more demanding abilities.",1,3,4,0,120,2,2,SkillKind::Restore},
        {8,"Bitter Prescription","A carefully measured poison keeps hurting after the immediate strike.",1,4,4,0,115,2,4,SkillKind::Poison},
        {9,"Mercy's Circle","Treat several companions standing near the selected patient.",1,6,4,1,145,3,7,SkillKind::Heal},
        {10,"Borrowed Pulse","Take vitality from an enemy and put it to more humane use.",1,6,4,0,155,3,10,SkillKind::Drain},
        {11,"Refuse the Ending","A powerful emergency treatment for a battered party.",1,8,5,2,210,4,13,SkillKind::Heal},
        {12,"Marked Card","A focused ranged strike from a performer who never leaves odds to chance.",2,3,5,0,140,1,1,SkillKind::Strike},
        {13,"Come Closer","Pull an enemy out of a comfortable position and into the party's reach.",2,4,5,0,100,2,2,SkillKind::Pull},
        {14,"Exit Through Velvet","Blink to a free tile as though stepping behind a stage curtain.",2,4,6,0,100,2,4,SkillKind::Blink},
        {15,"The Wrong Spotlight","Dazzle a threatening enemy before it can complete its act.",2,5,5,0,110,3,7,SkillKind::Stun},
        {16,"A Deck of Thorns","Poison a small group with an impossible spread of black cards.",2,6,5,1,135,3,10,SkillKind::Poison},
        {17,"Final Encore","Release a devastating flourish across the scene's final marked targets.",2,8,6,2,185,4,13,SkillKind::Strike}
    };
    return data;
}

const vector<Mission>& missions() {
    static const vector<Mission> data = [] {
        vector<Mission> result;
        // This small content builder keeps the long story readable. Every case
        // still supplies its own dialogue, evidence puzzle, and decision.
        auto add = [&](string name, string location, string summary, Objective objective,
                       vector<string> intro, vector<string> outro, array<string,2> choices,
                       string question, array<string,3> options, int answer, string explanation) {
            const int id = static_cast<int>(result.size()), chapter = id / 4;
            Mission m;
            m.id=id; m.chapter=chapter; m.name=name; m.location=location; m.summary=summary;
            m.biome=chapter; m.level=1+id/2; m.objective=objective;
            m.gold=50+id*8; m.xp=50+id*8;
            m.intro=move(intro); m.outro=move(outro); m.choices=move(choices);
            m.clueQuestion=question; m.clueOptions=move(options); m.clueAnswer=answer; m.clueExplanation=explanation;
            // Each district has a recognizable enemy ecology. Later districts
            // mix earlier specialists into formations, not just larger numbers.
            const array<array<int,6>,8> rosters{{
                {{0,1,2,3,4,5}}, {{3,4,5,6,7,0}}, {{8,9,10,11,4,6}}, {{12,13,14,15,8,10}},
                {{16,17,18,19,7,13}}, {{20,21,23,3,17,6}}, {{22,20,21,23,14,19}}, {{23,21,22,20,17,15}}}};
            const int count = chapter < 2 ? 4 : chapter < 5 ? 5 : 6;
            for (int wave=0; wave<2; ++wave)
                for (int i=0; i<count; ++i)
                    m.enemies[wave].push_back(rosters[chapter][static_cast<size_t>((i+id+wave*2)%6)]);
            if (id%4==3) {
                // A chapter boss is accompanied by two specialists. Its base
                // stats and AI are different from an ordinary recolored enemy.
                m.enemies[1]={24+chapter,rosters[chapter][1],rosters[chapter][3]};
            }
            result.push_back(move(m));
        };

        add("The Black Envelope","Vale Street sorting office","Recover the letter that remembers Mira.",Objective::Hunt,
            {"Rowan: My sister's death certificate arrived before anyone told me she was missing. Every space is blank except the municipal seal.",
             "Iona: Blank paper does not prove a death. It proves that someone was permitted to stop asking questions. We should begin with whoever delivered it.",
             "Silas: The sorting office is still open, although its clock stopped at midnight. Those porters are carrying envelopes that whisper the names of their recipients.",
             "A black envelope waits under a lamp that casts no shadow. The address is yours. The handwriting is Mira's, but the ink is still warm."},
            {"Inside is a brass button and a message: IF THEY ASK, I NEVER LEARNED TO SWIM. Mira has crossed out the word THEY three times.",
             "The clerk remembers accepting the envelope from a woman in a theatre mask. He cannot remember the woman's face because somebody paid him not to.",
             "Your first Bureau case number appears on the certificate by itself. The city has noticed that you are investigating."},
            {"Protect the frightened clerk's identity.","Sell the clerk's route to a competing broker."},
            "Which detail makes the death certificate unreliable?",{"Its expensive paper","The cause and identity are both missing","The envelope is black"},1,
            "A certificate without an identified deceased or a cause establishes no death. The seal is authority being used in place of evidence.");

        add("The Quiet Tenement","Bellwether Court","Guide the tenants past their own forgotten fears.",Objective::Escort,
            {"The residents of Bellwether Court have paid a broker to forget a recent fire. They still wake coughing. They simply no longer know why.",
             "A little girl recognizes Mira's brass button. She says the woman with the notebook taught her how to count the stairs when the stairs began moving.",
             "Iona: We can retrieve the evidence after the tenants are safe. A living witness is worth more than a perfect report.",
             "The hallway lamps go out in sequence. At the far end, three tenants wait beside a door that keeps returning them to their rooms."},
            {"The girl's exercise book contains drawings of nine bells. Eight have tongues. The ninth has a human eye where its tongue should hang.",
             "Silas: That is not a child's invention. Old theatres used the same symbol to mark a performance that required a witness.",
             "The tenants name an undertaker who bought the building's unclaimed possessions before the fire was officially reported."},
            {"Spend time arranging shelter for the tenants.","Accept the landlord's payment to close the complaint."},
            "What connects the fire to the undertaker?",{"He purchased possessions before the report","He owns a black coat","His office is nearby"},0,
            "Buying the belongings in advance suggests prior knowledge. His clothing and address do not establish involvement.");

        add("An Undertaker's Arithmetic","Hush and Son mortuary","Break the seals hiding a second set of names.",Objective::Seal,
            {"The undertaker insists that every coffin is occupied. His ledger lists thirty-one burials and thirty-two payments.",
             "Rowan: The extra payment came from the Bureau. It was authorized on the day Mira disappeared, using a signature that looks almost like mine.",
             "Iona finds ritual wax beneath the ledger's spine. Three account seals are keeping a second set of writing beneath the visible ink.",
             "Silas: Close the seals gently if you can. Whatever is written underneath has been trying very hard to remain a secret."},
            {"The hidden page lists people whose deaths were purchased, not people who died. Mira's entry reads TRANSFERRED, with a theatre ticket for an address.",
             "A note in the margin orders the ledger delivered to a funeral that has no deceased. The payment is a small bell wrapped in red cloth.",
             "Rowan: Someone is moving names through the city as if they were freight. We are going to their next collection."},
            {"Give the stolen names to the families first.","Keep the list as leverage over their employers."},
            "Why is the extra payment important?",{"It is a larger coin","There are more payments than burials","The ledger smells of wax"},1,
            "The ledger's arithmetic reveals a transaction without a burial. That contradiction leads to the concealed trade in identities.");

        add("Funeral Without Bells","Saint Oris's abandoned yard","Face the Mourning Beadle and recover the bell's receipt.",Objective::Hunt,
            {"No bell rings over the funeral yard. Mourners stand around an empty coffin, each holding a photograph with the face scratched away.",
             "The Mourning Beadle: You have arrived without an invitation. That means either someone still loves you, or your paperwork is incomplete.",
             "Rowan shows the ledger. The Beadle recognizes the handwriting and orders the mourners to turn their faces toward the ground.",
             "Mourning Beadle: Your sister bought a route into a place no officer can enter honestly. She paid with the first thing she ever remembered."},
            {"Beneath the coffin is a receipt for a funeral bell signed by Deputy Registrar Hester Quill. The bell was sent to Brass Market, not to a church.",
             "Iona returns the photographs to the mourners. Faces begin to reappear, imperfectly, as their owners speak the names aloud.",
             "Rowan: Mira used to remember the garden behind our first house. If she sold that memory, I may now be the only person who can tell her where she came from."},
            {"Let the mourners reclaim the bell's remaining memories.","Confiscate the memories as Bureau evidence."},
            "What was actually being buried?",{"An empty coffin","A plague victim","The identities in the photographs"},2,
            "The ritual erases people socially rather than killing their bodies. Restoring their names interrupts the funeral's real purpose.");

        add("Collateral Childhood","Brass Market pawn arcade","Recover pledged memories before the auction closes.",Objective::Hunt,
            {"Brass Market advertises loans without interest. The small print promises instead to collect one precious memory every year until the debt is paid.",
             "A mother recognizes Rowan's uniform and asks for help remembering her son's voice. She can afford the debt; she cannot afford the collector's fee.",
             "Silas: A dealer here once sold me an excellent childhood. It belonged to somebody else. I returned it, which made me very unpopular.",
             "Mira's ticket points to the arcade's locked collateral room. A row of mechanical apostles guards shelves full of other people's summers."},
            {"The collateral shelf holds a tin whistle, a red glove, and a jar labelled MIRA VALE: FIRST GARDEN. Its seal was broken from the inside.",
             "The broker claims that the garden memory escaped when a woman began asking who received the collected interest.",
             "A new receipt names the Yesterday Mint. Its currency is accepted only by people who have already forgotten what it costs."},
            {"Return the family's pledged memories.","Take a commission for recovering the collateral."},
            "Which object is directly connected to Mira?",{"The unlabelled glove","The jar bearing her name","The broker's desk bell"},1,
            "The labelled jar establishes a specific transaction. The other objects may matter to someone, but they do not yet connect to Mira.");

        add("Yesterday's Counterfeit","The Yesterday Mint","Hold the mint while Iona identifies the false memories.",Objective::Survive,
            {"Coins at the Yesterday Mint are stamped with moments instead of monarchs. A warm kitchen buys less than a first kiss, but more than an ordinary Tuesday.",
             "Iona finds identical childhood recollections in six different coins. Perfectly identical memories are not coincidence; someone is manufacturing witnesses.",
             "Rowan: If these coins can teach six strangers the same event, they could make a whole city remember a flood incorrectly.",
             "The mint's shutters fall. Keep the production floor while Iona separates the genuine recollections from the repeated lie."},
            {"The counterfeit moment shows Harrowmere's founder rescuing children from a flood. Every copied child has the same face.",
             "Silas knows that face from a theatre poster: a performer paid to stand in for someone the audience was never allowed to see.",
             "A delivery schedule sends the best counterfeits to an auction called The Last Lot. Its guest list includes Deputy Registrar Quill."},
            {"Destroy the false testimony and preserve the real donors.","Keep a batch to trade for access to the auction."},
            "How can Iona recognize a manufactured memory?",{"It has a shiny surface","It depicts a famous person","Several coins contain exactly the same private viewpoint"},2,
            "Independent memories have differences in attention and perspective. Identical private viewpoints indicate copying rather than corroboration.");

        add("A Smile Under Glass","The broker's winter gallery","Seal the display cases before their smiles find new owners.",Objective::Seal,
            {"The gallery's displays are full of smiles suspended in glass. Each is labelled with a donor's debt and the date they ceased to enjoy their own jokes.",
             "The proprietor offers Rowan a happy memory of Mira. It is a good imitation, except the sister in it calls him by the Bureau's formal name.",
             "Silas: A convincing lie is not obliged to be a complete one. Look for the thing the teller could not have known.",
             "Three display seals begin opening. Close them before the borrowed expressions settle onto the people waiting outside."},
            {"The false Mira speaks an address when the display closes: Berth Nine, Salt Docks. For a moment the voice becomes her real, impatient voice.",
             "The proprietor admits that Mira sold access to her recent memories to buy a place aboard a ship that appears on no harbor register.",
             "A gold invitation remains on the desk. The Grin Collector is expecting a sale; you can give him an investigation instead."},
            {"Release the donors' emotions without charging them.","Demand auction credentials before returning the displays."},
            "What exposes the offered memory as false?",{"Mira uses a name she never used privately","The gallery is cold","The proprietor smiles"},0,
            "The intimate detail conflicts with Rowan's direct experience. A cold room or a suspicious smile is not enough to establish a forgery.");

        add("The Last Lot","Midnight auction chamber","Defeat the Grin Collector before he sells Mira's route.",Objective::Hunt,
            {"The final auction lot is not a memory. It is permission to remember a particular woman who is supposed to have died without a name.",
             "Grin Collector: Your sister drove an excellent bargain. She did not ask to be rescued. She asked that someone follow after she stopped being able to return.",
             "Rowan sees Quill's chair standing empty. A wet handprint marks the catalogue where the deputy registrar's signature should be.",
             "The Collector's purchased expressions all begin laughing at once. The auction house will honor only the last bidder left standing."},
            {"The Collector's own ledger names the Ninth Witness, an office bought and sold once each generation. Mira has been nominated for it.",
             "A ship called The Patient Dawn carries witnesses from Berth Nine. Its captain receives payment for passengers who never disembark.",
             "Iona: A nomination is not consent. We are going to ask Mira what she chose, and who arranged the choices she was given."},
            {"Cancel the debt contracts attached to the auction.","Exchange the contracts for the Collector's emergency funds."},
            "Which discovery turns the search toward the docks?",{"The empty chair","The named ship and berth","The auction's high prices"},1,
            "A vessel name and a numbered berth are verifiable leads. The other observations establish atmosphere and suspicion, not a destination.");

        add("Cargo for the Dead","Berth Nine warehouses","Recover manifests addressed to people lost in the flood.",Objective::Hunt,
            {"Berth Nine smells of salt and old paper. The warehouse manifests are addressed to citizens recorded as drowned in the city's founding flood.",
             "A porter insists that the recipients collect their parcels every Thursday. He has never seen their faces; the windows are always fogged when they arrive.",
             "Rowan finds a fresh parcel for Mira. Inside is the brass button's matching coat, repaired with thread the color of dried seaweed.",
             "The harbor lights turn inward. Cargo handlers in sealed diving suits begin moving between the stacks without making a sound."},
            {"The manifests carry hospital ward numbers instead of street addresses. Each delivery is signed for by a different person using the same handwriting.",
             "Iona recognizes the old Mercy Hospital intake notation. The building was condemned twenty years ago, but someone has been ordering clean sheets.",
             "One parcel contains a letter that has not yet been posted. It is addressed to Rowan and dated tomorrow."},
            {"Give the dock families copies of the missing-person manifests.","Sell the manifests to a shipping rival for travel funds."},
            "What makes the delivery signatures suspicious?",{"They use blue ink","They share handwriting despite different names","They are written at night"},1,
            "A repeated hand signing for separate recipients suggests a common intermediary. It does not prove the recipients are alive or dead.");

        add("The Wet Post","Harrowmere dead-letter station","Escort the postmaster through letters that want to be read.",Objective::Escort,
            {"The postmaster has spent fifteen years delivering letters nobody remembers sending. Tonight he wants to retire, and the letters will not let him leave.",
             "Mira's unposted letter is already wet. It says the hospital is real, the flood is false, and Rowan must not trust any witness who has forgotten being afraid.",
             "Silas: Fear is unpleasant, but it is also an excellent record that something mattered. A broker who removes all fear can remove all objections with it.",
             "Bring the postmaster and his delivery key through the sorting yard before the dead letters find another address."},
            {"The postmaster gives Iona a route map with a lighthouse circled in red. He remembers delivering medicine there before the light was built.",
             "On the back, Mira has written the first line of a song Rowan taught her as a child. This message came from her, not a copied recollection.",
             "The old man keeps one letter. It is an apology to his daughter. He says he would rather remember writing it than be free of the shame."},
            {"Help the postmaster deliver his final apology.","Take the valuable route key and leave his letter behind."},
            "Which part of Mira's letter is the strongest identity check?",{"Its wet paper","Its date","The private childhood song"},2,
            "An independently known private detail helps authenticate the sender. Physical condition and a written date are easier to reproduce.");

        add("A Lighthouse in Daylight","The inland lighthouse","Keep the beacon steady while its hidden message is decoded.",Objective::Survive,
            {"The lighthouse stands a mile from the present coast. Its light sweeps across dry streets, marking doors as if they were the windows of sinking ships.",
             "Iona: The hospital map is older than the shoreline. Either the flood changed the city, or the city changed its account of the flood.",
             "Mira's signal is hidden in the intervals between the flashes. Silas needs the beacon running long enough to read the full sequence.",
             "Harbor lanterns unfold from the gallery railings. Their beams are trained on the lens you need to protect."},
            {"The signal gives the hospital's approach and a warning: THE ADMIRAL IS GUARDING THE WRONG PRISONER. ASK HIM WHO TAUGHT HIM TO SINK.",
             "The lighthouse log says its first keeper was a Bureau investigator. Every later keeper has copied that investigator's handwriting without noticing.",
             "Rowan: Someone has turned a confession into a public service. We should find the person who wrote it before we trust the light."},
            {"Publish the safe route for lost harbor workers.","Keep the route exclusive and charge a ferryman for it."},
            "Where is the signal encoded?",{"Between the flashes","In the lighthouse's height","In the paint on the door"},0,
            "The repeated timing pattern carries the message. The ordinary physical features are useful landmarks, not the encoded text.");

        add("The Captain Who Sank Twice","The Patient Dawn","Confront the Drowned Admiral and cross to Mercy.",Objective::Hunt,
            {"The Patient Dawn is waiting on a canal that is too narrow to contain it. Its passengers stare at the shore with the patience of people who have never been allowed to arrive.",
             "Drowned Admiral: I saved my crew from death. I cannot now permit them to remember what saving them required.",
             "Rowan asks who taught him to sink. The Admiral looks toward an empty captain's chair and says, very quietly, that the city paid him to practice.",
             "The mooring ropes tighten around the gangway. To reach Mercy Hospital, the party must break a captain's duty to the wrong authority."},
            {"The Admiral's second drowning was a ritual rehearsal. Harrowmere's first flood was arranged to create a generation of witnesses who could be bought.",
             "He releases the passengers' names before the ship dissolves into ordinary canal water. One name is Deputy Registrar Quill, listed as both passenger and owner.",
             "Iona finds a dry path where the gangway stood. At its far end a hospital window opens, and someone begins ringing for a nurse."},
            {"Help the passengers recover their own accounts of the flood.","Seize the ship's cargo before the survivors can claim it."},
            "What does Quill's double listing suggest?",{"A harmless spelling error","She controlled the voyage as well as joining it","She must be the captain"},1,
            "The records give Quill both a passenger role and ownership. That supports control of the operation, without making her its captain.");

        add("Ward of Borrowed Dreams","Mercy Hospital east ward","Identify and contain the dreams moving between patients.",Objective::Seal,
            {"Mercy Hospital's patients sleep beneath portraits of people they have never met. Every bed has a different occupant but the same dream written on its chart.",
             "Iona: These are not sedatives. Someone is teaching the patients a memory while they sleep, then recording their testimony when they wake.",
             "A physician's note says that Mira refused the first treatment and requested access to the donor register. The request bears Iona's former supervisor's seal.",
             "Three dream circuits pulse beneath the beds. Close them before the ward's shared recollection becomes permanent."},
            {"The patients wake remembering the same rescue from the flood. One remembers the scenery being painted between takes.",
             "Iona identifies the supervisor's notation: no patient may be treated without consent. The consent pages have all been cut away from their charts.",
             "Mira's request leads to a patient registered as NOBODY. Unlike the others, this patient has been awake for nineteen years."},
            {"Let the patients choose which memories to keep.","Take their prepared testimony as evidence without consulting them."},
            "What is missing from the medical records?",{"The hospital's emblem","A diagnosis written in red","The patients' consent pages"},2,
            "The missing consent removes the basis for the treatment's legitimacy. A complete medical record must preserve the patient's decision.");

        add("A Patient Called Nobody","The observation garden","Escort the nameless patient past the ward's keepers.",Objective::Escort,
            {"Nobody is sitting in a garden where the rain falls upward. She has been keeping a list of everything the hospital tried to make her forget.",
             "Nobody: They could take my name because I once believed they had the right. They could not take my list. I wrote it before I believed them.",
             "Her first entry describes a city official opening a sluice gate on the night of the flood. Her last describes Mira carrying the ninth bell into the surgical theatre.",
             "The orderlies will not permit an unnamed patient to be discharged. Bring her list through the garden before they make her sign again."},
            {"Outside the ward, Nobody chooses a new name: Elin. Iona writes it on a clean discharge page and asks for permission before signing as witness.",
             "Elin says the Suture Saint began by restoring memories to injured workers. Quill supplied donors, then decided what the restored workers should remember.",
             "Mira went to the theatre to find the original donor. Elin has been drawing his face for nineteen years."},
            {"Recognize Elin's chosen name in the official report.","Preserve the old designation to make the Bureau accept her evidence."},
            "Why did the private list survive the erasure?",{"It was written before the coerced consent","The ink was green","Nobody had no memories"},0,
            "The list is an external record created outside the later agreement. Its value is independent evidence, not a magical property of its ink.");

        add("The Theatre of Kindness","Mercy surgical theatre","Hold the operating room while Iona frees the donors.",Objective::Survive,
            {"The operating theatre contains no scalpels. Its instruments are tuning forks, glass needles, and a chair fitted with a bell-shaped frame.",
             "Iona recognizes her supervisor's first prototype. It was meant to help people describe trauma without reliving it. Someone has reversed the procedure.",
             "A donor strapped to the chair speaks with several voices. One is Mira's, asking whether a witness can testify against a city that owns her testimony.",
             "Keep the orderlies away while Iona disconnects the donors one memory at a time. A hurried rescue could leave them with another person's life."},
            {"The freed donors remember Quill removing a mask before every treatment. The face underneath was never the same twice.",
             "Silas knows the mask-maker's signature. It belongs to the Painted Mile, and to a troupe he swore he would never join again.",
             "Iona keeps the prototype's original instruction plate: ASK FIRST. LISTEN SECOND. DO NOT MISTAKE SILENCE FOR AGREEMENT."},
            {"Use the remaining medicine to stabilize the donors.","Reserve the medicine for the party's journey."},
            "What changed the prototype into an instrument of control?",{"Its age","The removal of consent and reversal of its purpose","The number of chairs"},1,
            "The same mechanism can help or harm depending on who controls it and whether the patient freely agrees. The records reveal a deliberate reversal.");

        add("The Suture Saint","The chapel of recovery","Break the Saint's false mercy and recover the theatre route.",Objective::Hunt,
            {"The Suture Saint is waiting beneath a chapel ceiling stitched from old patient gowns. The congregation thanks him for injuries they cannot recall receiving.",
             "Suture Saint: I took away the suffering. Why do you insist on giving it back?",
             "Iona: Because you also took away their chance to say what happened. Healing is not the same thing as making an account convenient.",
             "The Saint opens his hands and the orderlies kneel. His kindness has become a system, and the system is prepared to defend itself."},
            {"The Saint's donor register identifies the original witness as the city's first investigator. Every counterfeit recollection began with his carefully edited account.",
             "Mira followed the investigator's surviving face to the Painted Mile. She left Iona a note: YOU CAN CARE FOR PEOPLE WITHOUT KEEPING THEM QUIET.",
             "Iona closes the chapel ledger. She will not let the Saint's crimes make her ashamed of wanting to heal."},
            {"Leave the restored hospital under its patients' oversight.","Place the hospital's remaining stores under exclusive Bureau control."},
            "Why does the congregation's gratitude not settle the case?",{"They have been denied the memories needed for informed judgment","Gratitude is always false","The chapel is abandoned"},0,
            "A response formed after essential information was removed cannot replace informed consent. The party must preserve the patients' ability to decide.");

        add("The Empty Playbill","Painted Mile entrance","Recover a playbill for the performance Mira attended.",Objective::Hunt,
            {"Every theatre on the Painted Mile advertises the same play. The title has been erased, but the applause can be heard through locked doors.",
             "Silas: We used to tell ourselves an audience came here to escape its troubles. Then the management began selling the lives the audience escaped into.",
             "A playbill bears Mira's seat number. Her name is printed where the leading actor's should be, although rehearsals began before she was born.",
             "Paper acrobats fold themselves out of the posters. They are determined that every member of tonight's cast will take their assigned position."},
            {"The playbill is a contract disguised as a programme. Attending the final act permits the theatre to retain the audience's remembered faces.",
             "Mira crossed out the attendance clause and wrote WITNESS ONLY. The altered signature is why the troupe has not been able to claim her completely.",
             "Silas points to a stage-door mark he cut into the stone as a child. It is still a way in. He hopes it can still be a way out."},
            {"Warn the waiting audience about the contract.","Use their admission tickets to enter without a confrontation."},
            "What protected Mira from the attendance clause?",{"Her seat number","The missing title","Her explicit change to the contract"},2,
            "Mira did not accept the printed terms unchanged. Her written reservation is evidence that attendance did not mean unlimited consent.");

        add("Faces in the Dressing Room","The identity wardrobe","Seal the mirrors before the stored identities scatter.",Objective::Seal,
            {"The dressing room contains hundreds of labelled masks and one unlabelled mirror. Looking into it shows who the theatre wants you to become.",
             "Rowan sees a version of himself who signed Mira's death certificate willingly. Iona sees a physician who never asks difficult questions.",
             "Silas sees nothing. He says the mirror knows he has already played the part it would offer him.",
             "Close the wardrobe seals before the understudies step out of the mirrors wearing the party's better reputations."},
            {"One mask changes between all the faces seen on Deputy Registrar Quill. The deputy's authority is a role, passed between actors who need not share a body.",
             "Silas admits that he once wore a minor official's face to help the troupe move a prisoner. He did not learn the prisoner's name until it was too late.",
             "The prisoner's face is painted on Mira's marked playbill. The troupe has been staging the first investigator's testimony for generations."},
            {"Record Silas's confession without treating him as disposable.","Trade his old theatre identity for privileged access."},
            "What does the changing Quill mask establish?",{"The official role can be performed by different people","Every citizen is an actor","Quill never existed in any form"},0,
            "The evidence shows that an apparently continuous office is being impersonated. It does not erase all the individuals who performed it.");

        add("Silas Takes the Stage","The rehearsal hall","Keep Silas's route open while the old troupe assembles.",Objective::Survive,
            {"The rehearsal hall is lit for a reunion. Silas's old costume waits on a chair, with a note thanking him for returning the missing witness.",
             "Silas: They taught us to disappear through a curtain. They neglected to teach us what to do when the curtain was a person.",
             "Rowan offers him the back exit. Silas declines. He wants to finish the confession in a room where the people who taught him might hear it.",
             "The ushers lock the auditorium. Hold the hall while Silas dismantles the stage routes that let the troupe move prisoners through its scenery."},
            {"A rehearsal recording shows Mira entering the conspiracy voluntarily. She wanted the first investigator's unedited testimony and expected the Bureau to obstruct her.",
             "Her last clear instruction is addressed to Rowan: DO NOT RESCUE THE VERSION OF ME THEY ARE SELLING. FIND OUT WHAT I ACTUALLY ASKED FOR.",
             "Silas burns his old costume, keeping only the ordinary needle he used to mend it. Some tools deserve a less theatrical second life."},
            {"Give Silas time to help the trapped understudies leave.","Secure the best theatre equipment before the troupe escapes."},
            "What changes after hearing Mira's recording?",{"The investigation is no longer needed","Her choices must be established, not assumed","The Bureau was right to erase her"},1,
            "Voluntary entry does not justify later coercion. It does require Rowan to seek Mira's own wishes rather than inventing them for her.");

        add("An Applause of Teeth","The grand theatre","Defeat the Maestro of Borrowed Faces.",Objective::Hunt,
            {"The Maestro steps onto a stage built from the doors of houses nobody remembers living in. Every face he wears applauds the previous one.",
             "Maestro: I gave this city continuity. Mayors die, witnesses tire, families disagree. A role can serve forever if one removes the troublesome performer.",
             "Silas: A role that requires its performer to disappear is not a public service. It is a machine for escaping responsibility.",
             "The curtain rises on a painted flood. Behind it is a genuine staircase descending into the city's sealed archive."},
            {"The Maestro's final face is Deputy Registrar Quill's. Under that mask is a theatre inventory tag, not an answer about who first ordered the bargain.",
             "The archive staircase leads below the oldest part of the Bureau. Mira has left arrows in ordinary chalk, a material too cheap for the troupe to respect.",
             "Rowan: We have been following the people who repeat the lie. Now we find the document that taught it to them."},
            {"Restore the stolen faces to the surviving performers.","Retain the most useful official masks for the investigation."},
            "Why is the inventory tag significant?",{"It identifies the costume system behind Quill's appearances","It proves nobody made decisions","It makes the Maestro innocent"},0,
            "The tag explains how the role was maintained. It does not absolve the people who used that mechanism to exercise authority.");

        add("The City's First Lie","The Bureau's sealed basement","Recover the original founding report.",Objective::Hunt,
            {"The archive's oldest room is directly below the Bureau's public complaints desk. Citizens have been asking questions over the buried answers for a century.",
             "Iona finds the first investigator's report filed under WEATHER. Its summary describes a natural disaster; its diagrams show sluice gates opened by hand.",
             "An amendment names every person permitted to remember the distinction. The last name was added in Mira's handwriting.",
             "Blank inspectors move between the reading tables. Their reports have already concluded that no unauthorized person entered this room."},
            {"The original flood report describes a controlled inundation intended to create a collective memory strong enough to secure a civic bargain.",
             "The first investigator objected, then signed a revised report. His objection survives because a clerk refused to destroy the draft.",
             "Mira circled the clerk's name. Her search, like yours, keeps finding ordinary people whose smallest refusal preserved something essential."},
            {"Protect the clerk's descendants from official retaliation.","Use the family's hidden archive as leverage."},
            "What contradicts the report's natural-disaster summary?",{"The document's age","The hand-operated sluice diagrams","Its basement location"},1,
            "The diagrams describe human intervention. That concrete contradiction outweighs the official summary's unsupported classification.");

        add("A Flood on Paper","The revision chamber","Seal the amendments rewriting the survivors' accounts.",Objective::Seal,
            {"The revision chamber holds testimony on long ribbons of paper. Each time an amendment is stamped, a survivor's account changes in its own handwriting.",
             "Rowan reads a draft in which Mira returned home safely. He wants to believe it for a full, dangerous moment.",
             "Iona: A comforting record is still a forgery if it was made to replace a person's experience. We cannot cure this by choosing our favorite version.",
             "Three active stamps are revising the flood testimony. Seal them before the original objections vanish from the city altogether."},
            {"The amendments are authorized by Registrar Null, an office older than the Bureau itself. Its signature is a blank space the paper bends around.",
             "Silas finds the city's charter indexed under PROPERTY, not GOVERNMENT. Harrowmere was pledged as collateral in a transaction that has never been closed.",
             "A marginal note says the Ninth Witness may challenge the agreement, but only while retaining a memory not acquired through the city's market."},
            {"Preserve the contradictory accounts for public examination.","Keep only the clearest accusation to simplify the case."},
            "Why must conflicting drafts be preserved?",{"Contradictions can reveal how the account was changed","All versions must be equally true","More paper makes a stronger ritual"},0,
            "A sequence of conflicting drafts can show an edit's purpose and author. Preserving evidence is different from treating every version as correct.");

        add("The Locked Index","The ambulatory records vault","Escort the surviving index to a safe reading room.",Objective::Escort,
            {"The index is a cabinet on small brass legs. It has spent ninety years moving witnesses away from whichever shelf the Registrar is examining.",
             "It offers to cooperate if someone promises to read the names aloud instead of counting them as losses.",
             "Rowan recognizes the first family name. It belongs to the little girl from Bellwether Court, whose grandparents were removed from a flood compensation claim.",
             "Move the index through the vault. The shelves are closing around it, and several inspectors have decided that a walking cabinet has no right to appeal."},
            {"The safe reading room contains Mira's unfinished testimony. She kept one memory outside the market: a childhood song taught freely, without a transaction.",
             "Rowan remembers the same song. Their shared recollection is neither property nor proof of obedience; it is a relationship the charter cannot price.",
             "The index opens its final drawer. Inside is a foundry order for a human-shaped bell tongue, due before dawn."},
            {"Publish the uncounted names with their families' consent.","Use the index to claim the Bureau's unspent restitution fund."},
            "What makes the childhood song useful against the charter?",{"It is louder than a bell","It was freely shared outside the market","It has exactly nine verses"},1,
            "The contract's reach depends on acquisition through its system. A freely shared memory provides an independent account it does not own.");

        add("Registrar Null","The charter room","Defeat the Registrar and recover the foundry order.",Objective::Hunt,
            {"Registrar Null waits in a room without a chair. The office was designed for a function, not a person, and the function has forgotten the difference.",
             "Registrar Null: The city agreed. Every annual payment was made. An orderly system cannot be invalidated by a few regrettable particulars.",
             "Rowan: Those particulars have names. Several are standing in this room, and the rest have authorized us to ask on their behalf.",
             "The charter begins correcting the party's names on its cover. Keep your own account long enough to make the Registrar answer it."},
            {"The charter cannot be destroyed in the archive. Its binding is distributed among nine bells, and the ninth has never been given a final voice.",
             "Mira is at the foundry attempting to testify before that voice is cast. The party has reached the part of her plan she could not complete alone.",
             "Iona copies the appeal clause before the Registrar's ink dries. Even a cruel agreement may contain a promise its authors expected nobody to use."},
            {"Open the archive to independent witnesses.","Place the recovered charter under the party's exclusive custody."},
            "Why is destroying the room's charter copy insufficient?",{"The binding is also held in nine bells","The paper is expensive","The Registrar dislikes fire"},0,
            "The surviving contract is distributed across its ritual anchors. Removing a copy is not the same as ending the underlying agreement.");

        add("Workers of the Ninth Shift","The bell foundry yard","Escort the workers carrying evidence out of the furnace yard.",Objective::Escort,
            {"The foundry's night shift has lasted eleven days. Workers are paid in memories of rest, which makes it difficult to explain why they are so tired.",
             "A forewoman has hidden Mira's notebook inside an ordinary lunch tin. She asks for safe passage for her crew before she will hand it over.",
             "Silas: The notebook matters. So do the people who kept it from the furnace. We are not going to repeat the Bureau's arithmetic here.",
             "Iron penitents leave the cooling racks as the yard gates begin to close. Bring the workers and their evidence through together."},
            {"Mira's notebook identifies the bells by the memories each contains. The eighth preserves the city's fear. The ninth will preserve its ability to object.",
             "The forewoman says Mira has been delaying the casting by demanding corrections to her own name. It is an infuriatingly bureaucratic form of resistance.",
             "Rowan smiles for the first time since the sorting office. His sister has always known how to make an unkind process take longer."},
            {"Secure back pay and shelter for the exhausted crew.","Take the foundry's payroll to fund the final approach."},
            "Why are the memories of rest an abusive payment?",{"They replace rest without providing it","They cannot be counted","The workers dislike dreams"},0,
            "Remembering rest does not meet a body's need to rest. The payment conceals the harm instead of compensating for it.");

        add("The Furnace Choir","The casting floor","Hold the furnaces while their forced choir is disconnected.",Objective::Survive,
            {"The casting floor's heat is regulated by a choir. Every singer has been taught that stopping will cause the city to drown again.",
             "Iona reads the pressure gauges. The threat is a lie; the furnaces can be cooled safely if the voices are disconnected in order.",
             "Rowan hears the childhood song buried in the choir. Mira is using it to keep several other witnesses aware of where they are.",
             "Hold the floor while the crew closes the voice channels. The Iron Cantor has sent its penitents to restore the performance."},
            {"As the choir falls silent, individual voices return. None of them sounds like the perfectly harmonious instrument the foundry called them.",
             "Mira's voice gives Rowan the next route and asks him to bring Iona. She has learned that she cannot win an argument with the charter while it controls her breathing.",
             "The forewoman marks the safe channels with chalk. A worker's practical knowledge has done what a century of official reassurance did not."},
            {"Keep every singer free to choose whether to testify.","Record the choir's testimony before allowing them to leave."},
            "What establishes that the choir can stop safely?",{"The Cantor's promise","The pressure gauges and shutdown sequence","The beauty of the song"},1,
            "The crew has verifiable measurements and a controlled procedure. Those are stronger grounds than the authority that created the threat.");

        add("The Name in the Mould","The ninth mould chamber","Seal the mould before a witness becomes an instrument.",Objective::Seal,
            {"The ninth mould is shaped like a person holding a bell. Mira stands beside it, arguing with an attendant about a missing middle initial.",
             "Mira: I knew you would follow. I was less certain you would ask what I wanted before deciding what rescuing me should look like.",
             "Rowan: I have been getting better at that. What do you want? Mira asks him to keep the mould from closing while she finishes a testimony she still owns.",
             "Seal the casting channels. The attendant has run out of corrections to accept, and the Iron Cantor is coming to settle the account."},
            {"Iona frees Mira from the breathing frame. Mira gives her account directly: the first investigator bargained away the flood victims to save the rest of the city.",
             "The debt has been renewed ever since because every new official was taught that reopening it would make the original sacrifice meaningless.",
             "Silas: A bad bargain does not become kind because enough people have been made to live inside it. Let us go ask its first witness."},
            {"Respect Mira's decision to finish the testimony freely.","Demand that she surrender the testimony to the Bureau immediately."},
            "What is the real purpose of the mould?",{"To make a decorative statue","To cast a witness's ability to object into the ninth bell","To repair the eighth bell"},1,
            "The ninth bell is designed to absorb objection itself. Freeing the witness preserves the possibility of challenging the bargain.");

        add("The Iron Cantor","The tuning chamber","Defeat the Cantor before the bells speak as one.",Objective::Hunt,
            {"The Iron Cantor's body is a frame of tuning forks around a small, perfectly ordinary human throat. It speaks in the patient tone of someone explaining a necessary expense.",
             "Iron Cantor: The city requires one voice. Disagreement is waste. Memory is fuel. A finished bell makes an excellent citizen.",
             "Mira answers with the childhood song, badly and deliberately out of tune. The existing bells begin to disagree with each other.",
             "The party must break the Cantor's control before it can turn the discord into another measured, useful silence."},
            {"The unfinished ninth bell opens a path to the Pale Conservatory, where the first investigator has been tending the bargain's uncollected memories.",
             "Mira can keep the path open from the foundry. She insists that this is a task, not another sacrifice, and makes Rowan promise to come back with an answer.",
             "Rowan leaves her the brass button. She recognizes the repair she made to his coat, and that small, unpurchased recollection is enough to hold the door."},
            {"Return the foundry's stored memories to their surviving owners.","Retain a reserve to bargain with the final witness."},
            "Why does Mira deliberately sing out of tune?",{"She has forgotten the song","Discord interrupts the Cantor's enforced single voice","The bells prefer bad music"},1,
            "The Cantor's ritual depends on forced agreement. An independent, imperfect voice exposes that agreement as control rather than consent.");

        add("The Garden Without Dawn","Pale Conservatory outer glass","Recover the recollections still rooted in the bargain.",Objective::Hunt,
            {"The conservatory has no sun. Pale leaves unfold around glass fruits, each containing a moment that somebody once believed they could not bear to keep.",
             "Iona recognizes a patient's first breath after surgery. Silas recognizes the shame of a confession he has already made. Rowan recognizes the garden Mira sold.",
             "A gardener asks whether they want their happiness back. His shadow is shaped like the first investigator's signature.",
             "The garden's custodians move to harvest the party's hesitation. Reach the rooted recollections before they are pruned into something easier to own."},
            {"The fruits do not offer complete lives, only the pieces the market was able to purchase. None can tell the party who they must become next.",
             "Mira's garden memory contains Rowan teaching her the same song that kept the foundry door open. A freely given relationship survived on both sides of the sale.",
             "The gardener directs the party toward an assembly of witnesses. For the first time, he sounds afraid that they may be permitted to speak."},
            {"Let the recovered memories return without demanding payment.","Keep the strongest recollections as protection for the final fight."},
            "Why is a recovered memory not a complete definition of its owner?",{"A person has continued living beyond that one moment","Memories are always false","Only happy moments matter"},0,
            "A memory is part of a life, not an order to repeat it. Restoring it should expand a person's choices rather than prescribe their identity.");

        add("The Assembly of the Forgotten","The witness pavilion","Escort the witnesses to a place where the charter can hear them.",Objective::Escort,
            {"The forgotten are holding a meeting without a chairperson. It has taken ninety years because the bargain taught each of them to wait for permission.",
             "Rowan lays down his Bureau badge before asking to speak. He has evidence to offer, but no authority to decide their wishes for them.",
             "Elin's list, the postmaster's letter, the clerk's draft, and the foundry notebook become different accounts of the same long deception.",
             "Bring the assembly's chosen witnesses through the pavilion. The charter's last inspectors are trying to classify them as property before they can make an appeal."},
            {"The witnesses do not agree on everything. Some want their old lives; some want new names; some want restitution without being required to forgive.",
             "Iona: That disagreement is what a living city sounds like. We have spent too long mistaking quiet for recovery.",
             "The assembly authorizes Rowan to challenge the bargain. It does not authorize him to replace it with another bargain made on their behalf."},
            {"Carry the witnesses' differing wishes into the appeal.","Reduce the appeal to a single demand that is easier to enforce."},
            "What gives Rowan the right to present the appeal?",{"His uniform alone","The witnesses' specific authorization","The number of enemies he defeated"},1,
            "Authority to speak for others comes from their authorization. A title or tactical victory does not replace that agreement.");

        add("Mira's Last Testimony","The conservatory bellwalk","Keep the appeal open until Mira's unowned account arrives.",Objective::Survive,
            {"The bellwalk connects the conservatory to the foundry for only as long as Mira can keep her testimony outside the charter's language.",
             "Her voice describes no heroic secret. She describes a clerk preserving a draft, a worker hiding a notebook, and a brother who finally learned to ask.",
             "The Vicar of the Hollow Hour interrupts from behind the last door. He says he remembers making all the same objections, before he learned what a city costs.",
             "Hold the bellwalk until the testimony is complete. The appeal needs a living account, not another beautifully edited sacrifice."},
            {"Mira finishes by stating her own name and declining the office of Ninth Witness. She offers testimony, not ownership of herself.",
             "The final door opens because the charter cannot find a clause that turns a freely given account into a sale. The Vicar has been avoiding that distinction for a century.",
             "Silas checks his remaining cards. Iona closes her casebook. Rowan picks up the badge again, now carrying it as a responsibility rather than an answer."},
            {"Accept Mira's testimony on the terms she chose.","Try to bind her account permanently to the Bureau's case."},
            "What distinction finally opens the door?",{"A freely given testimony is not a transfer of ownership","A longer statement is always stronger","The Vicar forgot to lock it"},0,
            "Mira supplies evidence without surrendering herself. The contract has relied on confusing those two acts throughout the investigation.");

        add("When the Ninth Bell Rings","The hollow-hour sanctuary","Face the first investigator and end the collection.",Objective::Hunt,
            {"The Vicar of the Hollow Hour is not wearing a mask. He is an old investigator with ink on his fingers, still standing beside the decision he made during the flood.",
             "Vicar: I saved a city. Every year I found a way to save it again. If I stop now, what were all those losses for?",
             "Rowan: The losses do not need another victim to become real. They need someone to stop calling them necessary after the need has passed.",
             "The bells speak in every voice the party has recovered. The final fight is for the right to let those voices remain separate, imperfect, and alive."},
            {"When the ninth bell rings, it rings without a tongue. The sound is carried by people saying their own names, and no office can collect the echo.",
             "Mira is waiting at the foundry door. She does not remember every garden, and Rowan cannot restore every lost afternoon. They go home with enough truth to begin talking.",
             "Iona opens a clinic that keeps its consent pages. Silas teaches stagecraft without selling identities. The Bureau receives a public reading room and a great many difficult questions.",
             "Harrowmere has not become innocent. It has become a city that can argue about what it owes. Tomorrow will be harder to administer, and much better to live in."},
            {"End the collection and let the city remember its witnesses.","Claim the charter's remaining power to force a quicker reconstruction."},
            "What was the Vicar's central mistake?",{"He cared about the city's survival","He treated past losses as a reason to require more victims","He wrote too many reports"},1,
            "Past sacrifices cannot justify continuing avoidable harm. Acknowledging the dead is different from using them to silence the living.");

        return result;
    }();
    return data;
}

bool validateContent(string& reason) {
    // Content is executable data: a bad enemy reference can be just as harmful
    // as a bad line of code. Validate it before a player invests in a campaign.
    auto fail = [&](const string& message) { reason=message; return false; };
    if (enemies().size()!=32 || items().size()!=48 || skills().size()!=SkillCount || missions().size()!=MissionCount)
        return fail("The campaign has an incomplete content table.");
    set<string> names;
    for (size_t i=0; i<enemies().size(); ++i) {
        const auto& e=enemies()[i];
        if (e.id!=static_cast<int>(i) || e.name.empty() || e.description.empty() || e.hp<=0 || e.attack<=0 || e.move<=0 || !names.insert(e.name).second)
            return fail("An enemy has an invalid ID, statistic, or duplicate name.");
        if (e.boss != (i>=24)) return fail("Bosses must occupy enemy IDs 24 through 31.");
    }
    names.clear();
    for (size_t i=0; i<items().size(); ++i) {
        const auto& item=items()[i];
        if (item.id!=static_cast<int>(i) || item.name.empty() || item.description.empty() || item.price<0 || item.unlockChapter<0 || item.unlockChapter>7 || !names.insert(item.name).second)
            return fail("An item has invalid data or a duplicate name.");
        if ((i<24)!=(item.slot==Slot::Weapon)) return fail("The first 24 items must be weapons.");
    }
    for (size_t i=0; i<skills().size(); ++i) {
        const auto& skill=skills()[i];
        if (skill.id!=static_cast<int>(i) || skill.hero!=static_cast<int>(i/6) || skill.name.empty() || skill.focusCost<0 || skill.unlockLevel<1 || skill.unlockLevel>16 || skill.range<1)
            return fail("A skill has invalid ownership or requirements.");
    }
    names.clear();
    for (size_t i=0; i<missions().size(); ++i) {
        const auto& mission=missions()[i];
        if (mission.id!=static_cast<int>(i) || mission.chapter!=static_cast<int>(i/4) || mission.biome!=mission.chapter || mission.level!=1+static_cast<int>(i/2) || !names.insert(mission.name).second)
            return fail("A case has invalid ordering or a duplicate name.");
        if (mission.intro.size()<3 || mission.outro.size()<3 || mission.clueQuestion.empty() || mission.clueExplanation.empty() || mission.gold<=0 || mission.xp<=0 || mission.clueAnswer<0 || mission.clueAnswer>2)
            return fail("A case is missing story, reward, or evidence content.");
        for (const auto& option:mission.clueOptions) if (option.empty()) return fail("An evidence puzzle has an empty answer.");
        for (const auto& choice:mission.choices) if (choice.empty()) return fail("A case is missing a decision.");
        for (const auto& wave:mission.enemies) {
            if (wave.empty()) return fail("A case has an empty encounter.");
            for (int id:wave) if (id<0 || id>=static_cast<int>(enemies().size())) return fail("A case refers to an enemy that does not exist.");
        }
        if (i%4==3 && mission.enemies[1][0]!=24+mission.chapter) return fail("A chapter is missing its own final boss.");
    }
    reason.clear();
    return true;
}
} // namespace veil
